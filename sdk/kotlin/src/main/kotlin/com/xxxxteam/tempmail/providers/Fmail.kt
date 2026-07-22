package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Fmail 渠道（fmail.men）。
 *
 * GET /v1/random 获取随机邮箱，GET /v1/inbox/{local} 收信，GET /v1/email/{token} 取详情。
 */
object Fmail : Provider {

    private const val BASE_URL = "https://fmail.men"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    )

    /** 创建临时邮箱（域名由服务端随机分配）。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE_URL/v1/random", HEADERS)
        resp.ensureSuccess()
        val obj = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("fmail: invalid random response")
        var email = jsonStr(obj, "address").trim()
        if (email.isBlank()) {
            val username = jsonStr(obj, "username").trim()
            val dom = jsonStr(obj, "domain").trim()
            if (username.isNotBlank() && dom.isNotBlank()) email = "$username@$dom"
        }
        if (email.isBlank() || !email.contains("@")) {
            throw RuntimeException("fmail: invalid random response")
        }
        return EmailInfo(email = email, channel = "fmail")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val value = info.email.trim()
        val atIdx = value.indexOf('@')
        if (atIdx < 0) throw RuntimeException("fmail: invalid email")
        val local = value.substring(0, atIdx)
        val domain = value.substring(atIdx + 1)
        val resp = ProviderUtil.httpGet(
            "$BASE_URL/v1/inbox/${ProviderUtil.urlEncode(local)}" +
                "?domain=${ProviderUtil.urlEncode(domain)}&limit=50", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val rows = root["emails"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { row ->
            val token = ProviderUtil.firstNonEmpty(jsonStr(row, "token"), jsonStr(row, "id")).trim()
            val src = if (token.isEmpty()) row else fetchDetail(token) ?: row
            Normalize.fromMap(flatten(src, value), value)
        }
    }

    /** 拉取单封邮件详情，失败返回 null。 */
    private suspend fun fetchDetail(token: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet("$BASE_URL/v1/email/${ProviderUtil.urlEncode(token)}", HEADERS)
            resp.ensureSuccess()
            val obj = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return null
            (obj["email"] as? JsonObject) ?: obj
        } catch (_: Exception) {
            null
        }
    }

    /** 扁平化消息字段。 */
    private fun flatten(raw: JsonObject, recipient: String): Map<String, Any?> {
        val to = ProviderUtil.firstNonEmpty(jsonStr(raw, "recipient"), jsonStr(raw, "to"))
        return mapOf(
            "id" to ProviderUtil.firstNonEmpty(jsonStr(raw, "id"), jsonStr(raw, "uid"), jsonStr(raw, "token")),
            "from" to ProviderUtil.firstNonEmpty(jsonStr(raw, "sender"), jsonStr(raw, "from")),
            "to" to (to.ifEmpty { recipient }),
            "subject" to jsonStr(raw, "subject"),
            "text" to ProviderUtil.firstNonEmpty(jsonStr(raw, "body_text"), jsonStr(raw, "text"), jsonStr(raw, "snippet")),
            "html" to ProviderUtil.firstNonEmpty(jsonStr(raw, "body_html"), jsonStr(raw, "html")),
            "date" to ProviderUtil.firstNonEmpty(jsonStr(raw, "received_at"), jsonStr(raw, "timestamp"), jsonStr(raw, "date")),
        )
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
