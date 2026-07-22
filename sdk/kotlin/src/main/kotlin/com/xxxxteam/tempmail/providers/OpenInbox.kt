package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * OpenInbox 渠道（openinbox.io）。
 *
 * POST /api/inbox 创建收件箱，token 为 inbox id；
 * GET /api/emails/inbox/{id} 获取列表，GET /api/emails/{id} 获取详情。
 */
object OpenInbox : Provider {

    private const val API_BASE = "https://api.openinbox.io/api"

    private fun headers(): Map<String, String> = mapOf(
        "Accept" to "application/json, text/plain, */*",
        "Origin" to "https://openinbox.io",
        "Referer" to "https://openinbox.io/",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val h = headers() + ("Content-Type" to "application/json")
        val resp = ProviderUtil.httpPost("$API_BASE/inbox", null, "application/json", h)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("openinbox: 响应格式无效")
        val inboxId = jsonStr(data, "id").trim()
        val email = jsonStr(data, "email").trim()
        if (inboxId.isEmpty() || email.isEmpty() || !email.contains("@")) {
            throw RuntimeException("openinbox: 响应数据无效")
        }
        return EmailInfo(email = email, channel = "openinbox", token = inboxId)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val inboxId = info.token.trim()
        if (inboxId.isBlank()) throw RuntimeException("openinbox: 空 inbox id")
        val email = info.email.trim()
        val resp = ProviderUtil.httpGet(
            "$API_BASE/emails/inbox/${ProviderUtil.urlEncode(inboxId)}?page=1&limit=50", headers(),
        )
        resp.ensureSuccess()
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val rows = body["emails"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { item ->
            val msgId = jsonStr(item, "id").trim()
            val src = if (msgId.isNotEmpty()) fetchDetail(msgId) ?: item else item
            val flat = mapOf(
                "id" to jsonStr(src, "id"),
                "from" to jsonStr(src, "from"),
                "to" to email,
                "subject" to jsonStr(src, "subject"),
                "body" to ProviderUtil.firstNonEmpty(jsonStr(src, "textBody"), jsonStr(src, "htmlBody")),
                "date" to jsonStr(src, "receivedAt"),
            )
            Normalize.fromMap(flat, email)
        }
    }

    /** 拉取单封邮件详情。 */
    private suspend fun fetchDetail(messageId: String): JsonObject? {
        if (messageId.isEmpty()) return null
        return try {
            val resp = ProviderUtil.httpGet("$API_BASE/emails/${ProviderUtil.urlEncode(messageId)}", headers())
            if (!resp.isOk) return null
            Http.json.parseToJsonElement(resp.body) as? JsonObject
        } catch (_: Exception) {
            null
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
