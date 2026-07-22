package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * UnCorreoTemporal 渠道（uncorreotemporal.com）。
 *
 * POST /api/v1/mailboxes 创建，token 为 session_token；
 * GET /api/v1/mailboxes/{email}/messages 收信，携带 X-Session-Token。
 */
object UnCorreoTemporal : Provider {

    private const val API_BASE = "https://uncorreotemporal.com/api/v1"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "Origin" to "https://uncorreotemporal.com",
        "Referer" to "https://uncorreotemporal.com/",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val h = HEADERS + ("Content-Type" to "application/json")
        val resp = ProviderUtil.httpPost("$API_BASE/mailboxes", null, null, h)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("uncorreotemporal: 创建邮箱响应无效")
        val email = jsonStr(data, "address").trim()
        val token = jsonStr(data, "session_token").trim()
        if (email.isEmpty() || !email.contains("@") || token.isEmpty()) {
            throw RuntimeException("uncorreotemporal: 创建邮箱响应无效")
        }
        return EmailInfo(email = email, channel = "uncorreotemporal", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val sessionToken = info.token.trim()
        val address = info.email.trim()
        if (sessionToken.isEmpty()) throw RuntimeException("uncorreotemporal: session token 为空")
        if (address.isEmpty()) throw RuntimeException("uncorreotemporal: 邮箱地址为空")
        val h = HEADERS + ("X-Session-Token" to sessionToken)
        val resp = ProviderUtil.httpGet(
            "$API_BASE/mailboxes/${ProviderUtil.urlEncode(address)}/messages?limit=50", h,
        )
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { item ->
            val msgId = jsonStr(item, "id").trim()
            val src = if (msgId.isNotEmpty()) fetchDetail(address, sessionToken, msgId) ?: item else item
            Normalize.fromMap(buildRaw(src, address), address)
        }
    }

    /** 拉取单封邮件详情。 */
    private suspend fun fetchDetail(email: String, token: String, messageId: String): JsonObject? {
        return try {
            val h = HEADERS + ("X-Session-Token" to token)
            val resp = ProviderUtil.httpGet(
                "$API_BASE/mailboxes/${ProviderUtil.urlEncode(email)}/messages/${ProviderUtil.urlEncode(messageId)}",
                h,
            )
            if (!resp.isOk) return null
            Http.json.parseToJsonElement(resp.body) as? JsonObject
        } catch (_: Exception) {
            null
        }
    }

    /** 归一化字段名（补齐 from/to/text/html/date 候选）。 */
    private fun buildRaw(obj: JsonObject, recipient: String): Map<String, Any?> = mapOf(
        "id" to jsonStr(obj, "id"),
        "from" to ProviderUtil.firstNonEmpty(jsonStr(obj, "from"), jsonStr(obj, "from_address")),
        "to" to ProviderUtil.firstNonEmpty(jsonStr(obj, "to"), jsonStr(obj, "to_address"), recipient),
        "subject" to jsonStr(obj, "subject"),
        "body" to ProviderUtil.firstNonEmpty(
            jsonStr(obj, "text"), jsonStr(obj, "body_text"),
            jsonStr(obj, "html"), jsonStr(obj, "body_html"),
        ),
        "date" to ProviderUtil.firstNonEmpty(jsonStr(obj, "date"), jsonStr(obj, "received_at")),
    )
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
