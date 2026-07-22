package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * smail.pw 渠道。
 *
 * POST _root.data 创建邮箱，GET _root.data 获取邮件列表，
 * GET /api/email/{id} 获取单封邮件正文。token 为 __session cookie。
 */
object SmailPw : Provider {

    private const val CHANNEL = "smail-pw"
    private const val ROOT_DATA_URL = "https://smail.pw/_root.data"

    private val QUOTED_INBOX = Regex("(?i)\"([a-z0-9][a-z0-9.\\-]*@smail\\.pw)\"")
    private val PLAIN_INBOX = Regex("(?i)\\b([a-z0-9][a-z0-9.\\-]*@smail\\.pw)\\b")
    private val MAIL_RE = Regex(
        "\"id\",\"([^\"]+)\",\"to_address\",\"([^\"]*)\",\"from_name\",\"([^\"]*)\"," +
            "\"from_address\",\"([^\"]*)\",\"subject\",\"([^\"]*)\",\"time\",(\\d+)",
    )
    private val MAIL2_RE = Regex(
        "\"id\",\"([^\"]+)\",\"from_name\",\"([^\"]*)\",\"from_address\",\"([^\"]*)\"," +
            "\"subject\",\"([^\"]*)\",\"time\",(\\d+)",
    )
    private val SESSION_RE = Regex("__session=([^;]+)")

    private fun defaultHeaders(): MutableMap<String, String> = linkedMapOf(
        "Accept" to "*/*",
        "accept-language" to "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "cache-control" to "no-cache",
        "dnt" to "1",
        "origin" to "https://smail.pw",
        "pragma" to "no-cache",
        "referer" to "https://smail.pw/",
        "sec-ch-ua" to "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        "sec-ch-ua-mobile" to "?0",
        "sec-ch-ua-platform" to "\"Windows\"",
        "sec-fetch-dest" to "empty",
        "sec-fetch-mode" to "cors",
        "sec-fetch-site" to "same-origin",
    )

    /** 提取 __session cookie。 */
    private fun extractSessionCookie(setCookies: List<String>): String {
        for (sc in setCookies) {
            if (sc.contains("__session=")) {
                val start = sc.indexOf("__session=") + "__session=".length
                val end = sc.indexOf(';', start)
                val v = if (end > 0) sc.substring(start, end) else sc.substring(start)
                return "__session=$v"
            }
        }
        return ""
    }

    /** 从文本中提取邮箱地址。 */
    private fun extractInbox(text: String): String =
        QUOTED_INBOX.find(text)?.groupValues?.get(1)
            ?: PLAIN_INBOX.find(text)?.groupValues?.get(1)
            ?: ""

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val h = defaultHeaders().apply {
            put("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8")
        }
        val resp = ProviderUtil.httpPost(
            ROOT_DATA_URL, "intent=generate", "application/x-www-form-urlencoded;charset=UTF-8", h,
        )
        resp.ensureSuccess()
        val cookie = extractSessionCookie(resp.setCookies)
        if (cookie.isEmpty()) throw RuntimeException("smail-pw: 未提取到 __session cookie")
        val email = extractInbox(resp.body)
        if (email.isEmpty()) throw RuntimeException("smail-pw: 无法从响应中解析邮箱地址")
        return EmailInfo(email = email, channel = CHANNEL, token = cookie)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token
        if (token.isBlank()) throw RuntimeException("smail-pw: token 不能为空")
        val email = info.email
        val h = defaultHeaders().apply { put("Cookie", token) }
        val resp = ProviderUtil.httpGet(ROOT_DATA_URL, h)
        resp.ensureSuccess()
        val rawList = parseMails(resp.body, email)
        return rawList.map { raw ->
            var em = Normalize.fromMap(raw, email)
            val id = em.id
            if (id.isNotEmpty() && !id.startsWith("__smail_")) {
                val body = fetchEmailBody(token, id)
                if (body.isNotEmpty()) em = em.copy(body = body)
            }
            em
        }
    }

    /** 拉取单封邮件正文。 */
    private suspend fun fetchEmailBody(token: String, mailId: String): String {
        return try {
            val h = defaultHeaders().apply {
                put("Cookie", token)
                put("Accept", "application/json")
            }
            val resp = ProviderUtil.httpGet("https://smail.pw/api/email/${ProviderUtil.urlEncode(mailId)}", h)
            if (!resp.isOk) return ""
            val data = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return ""
            jsonStr(data, "body")
        } catch (_: Exception) {
            ""
        }
    }

    /** 从响应文本中解析邮件（正则 + JSON 双路径去重）。 */
    private fun parseMails(text: String, recipient: String): List<Map<String, Any?>> {
        val seenIds = LinkedHashSet<String>()
        val all = mutableListOf<Map<String, Any?>>()

        for (m in MAIL_RE.findAll(text)) {
            val id = m.groupValues[1]
            if (seenIds.add(id)) {
                all.add(
                    mapOf(
                        "id" to id,
                        "to" to m.groupValues[2].ifEmpty { recipient },
                        "from" to m.groupValues[4],
                        "subject" to m.groupValues[5],
                        "date" to m.groupValues[6],
                    ),
                )
            }
        }
        for (m in MAIL2_RE.findAll(text)) {
            val id = m.groupValues[1]
            if (seenIds.add(id)) {
                all.add(
                    mapOf(
                        "id" to id,
                        "to" to recipient,
                        "from" to m.groupValues[3],
                        "subject" to m.groupValues[4],
                        "date" to m.groupValues[5],
                    ),
                )
            }
        }

        try {
            Http.json.parseToJsonElement(text).let { walkJsonEmails(it, recipient, all, seenIds) }
        } catch (_: Exception) {
        }
        return all
    }

    /** 递归遍历 JSON 提取邮件对象。 */
    private fun walkJsonEmails(
        node: JsonElement,
        recipient: String,
        out: MutableList<Map<String, Any?>>,
        seenIds: LinkedHashSet<String>,
    ) {
        when (node) {
            is JsonArray -> node.forEach { walkJsonEmails(it, recipient, out, seenIds) }
            is JsonObject -> {
                if (node["subject"] is JsonPrimitive && node["time"] != null) {
                    val id = jsonStr(node, "id")
                    val timeStr = jsonStr(node, "time")
                    if (timeStr.toLongOrNull() != null) {
                        var subject = jsonStr(node, "subject")
                        if (subject == recipient || subject.endsWith("@smail.pw")) subject = ""
                        if (id.isNotEmpty() && seenIds.add(id)) {
                            out.add(
                                mapOf(
                                    "id" to id,
                                    "to" to jsonStr(node, "to_address").ifEmpty { recipient },
                                    "from" to jsonStr(node, "from_address"),
                                    "subject" to subject,
                                    "date" to timeStr,
                                ),
                            )
                        }
                    }
                }
                for ((_, v) in node) {
                    if (v is JsonObject || v is JsonArray) walkJsonEmails(v, recipient, out, seenIds)
                }
            }
            else -> {}
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
