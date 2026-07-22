package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.net.URLDecoder
import java.nio.charset.StandardCharsets

/**
 * Emailnator 渠道（www.emailnator.com）。
 *
 * 需要 XSRF-TOKEN Session 认证。GET / 初始化 session →
 * POST /generate-email 创建邮箱 → POST /message-list 获取邮件。
 * token 为 JSON 编码的 {xsrfToken, cookies}。
 */
object Emailnator : Provider {

    private const val BASE_URL = "https://www.emailnator.com"

    private fun defaultHeaders(): Map<String, String> = mapOf(
        "User-Agent" to "Mozilla/5.0",
        "Accept" to "application/json",
        "Content-Type" to "application/json",
        "X-Requested-With" to "XMLHttpRequest",
    )

    /** 初始化 session，返回 (xsrfToken, cookieStr)。 */
    private suspend fun initSession(): Pair<String, String> {
        val h = mapOf("User-Agent" to "Mozilla/5.0", "Accept" to "text/html")
        val resp = ProviderUtil.httpGet(BASE_URL, h)
        resp.ensureSuccess()
        var xsrfToken = ""
        val cookies = StringBuilder()
        for (sc in resp.setCookies) {
            val seg = sc.split(";")[0].trim()
            if (cookies.isNotEmpty()) cookies.append("; ")
            cookies.append(seg)
            if (seg.startsWith("XSRF-TOKEN=")) {
                val encoded = seg.substring("XSRF-TOKEN=".length)
                xsrfToken = URLDecoder.decode(encoded, StandardCharsets.UTF_8)
            }
        }
        if (xsrfToken.isEmpty()) throw RuntimeException("emailnator: 无法提取 XSRF-TOKEN")
        return xsrfToken to cookies.toString()
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val (xsrfToken, cookies) = initSession()
        val h = defaultHeaders() + mapOf("X-XSRF-TOKEN" to xsrfToken, "Cookie" to cookies)
        val payload = buildJsonObject {
            putJsonArray("email") {
                add("plusGmail"); add("dotGmail"); add("googleMail")
            }
        }.toString()
        val resp = ProviderUtil.httpPost("$BASE_URL/generate-email", payload, "application/json", h)
        resp.ensureSuccess()
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("emailnator: 响应格式异常")
        val emailArr = body["email"] as? JsonArray
        if (emailArr.isNullOrEmpty()) throw RuntimeException("emailnator: 响应中无邮箱地址")
        val email = (emailArr[0] as? JsonPrimitive)?.contentOrNull ?: ""
        val token = buildJsonObject {
            put("xsrfToken", xsrfToken)
            put("cookies", cookies)
        }.toString()
        return EmailInfo(email = email, channel = "emailnator", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token
        if (token.isBlank()) throw RuntimeException("emailnator: token 为空")
        val tok = Http.json.parseToJsonElement(token) as? JsonObject
            ?: throw RuntimeException("emailnator: token 格式无效")
        val xsrfToken = jsonStr(tok, "xsrfToken")
        val cookies = jsonStr(tok, "cookies")
        val h = defaultHeaders() + mapOf("X-XSRF-TOKEN" to xsrfToken, "Cookie" to cookies)
        val email = info.email

        val payload = buildJsonObject { put("email", email) }.toString()
        val resp = ProviderUtil.httpPost("$BASE_URL/message-list", payload, "application/json", h)
        resp.ensureSuccess()
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val messageData = body["messageData"] as? JsonArray ?: return emptyList()
        return messageData.mapNotNull { it as? JsonObject }.mapNotNull { msg ->
            val msgId = jsonStr(msg, "messageID")
            if (msgId.startsWith("ADS")) return@mapNotNull null
            var html = ""
            if (msgId.isNotEmpty()) {
                try {
                    val detPayload = buildJsonObject {
                        put("email", email)
                        put("messageID", msgId)
                    }.toString()
                    val dr = ProviderUtil.httpPost("$BASE_URL/message-list", detPayload, "application/json", h)
                    if (dr.isOk) html = dr.body
                } catch (_: Exception) {
                }
            }
            val row = mapOf(
                "id" to msgId,
                "from" to jsonStr(msg, "from"),
                "to" to email,
                "subject" to jsonStr(msg, "subject"),
                "body" to html,
                "date" to jsonStr(msg, "time"),
            )
            Normalize.fromMap(row, email)
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
