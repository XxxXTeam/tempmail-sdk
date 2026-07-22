package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * TempMailPro 渠道 — https://tempmailpro.us
 *
 * POST /api/v1/mailbox/create 创建邮箱，GET /api/v1/mailbox/{token}/emails 获取邮件。
 */
object TempMailPro : Provider {

    private const val CHANNEL = "tempmailpro"
    private const val API_BASE = "https://tempmailpro.us/api/v1"

    private val headers = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val h = headers + ("Content-Type" to "application/json")
        val resp = ProviderUtil.httpPost("$API_BASE/mailbox/create", "{}", "application/json", h)
        resp.ensureSuccess()
        val box = ProviderUtil.parseObject(resp.body)?.get("data") as? JsonObject
        val email = box?.let { ProviderUtil.str(it, "address").trim() } ?: ""
        val token = box?.let { ProviderUtil.str(it, "token").trim() } ?: ""
        if (email.isEmpty() || !email.contains("@") || token.isEmpty()) {
            throw RuntimeException("tempmailpro: 创建邮箱响应无效")
        }
        return EmailInfo(email = email, channel = CHANNEL, token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val mailboxToken = info.token.trim()
        val address = info.email.trim()
        if (mailboxToken.isEmpty()) throw RuntimeException("tempmailpro: token 为空")
        if (address.isEmpty()) throw RuntimeException("tempmailpro: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$API_BASE/mailbox/${ProviderUtil.urlEncode(mailboxToken)}/emails", headers)
        resp.ensureSuccess()
        val rows = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "data") ?: return emptyList()
        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val msgId = ProviderUtil.str(item, "id").trim()
            val detail = if (msgId.isNotEmpty()) fetchDetail(mailboxToken, msgId) else null
            result.add(Normalize.fromMap(buildRaw(detail ?: item, address), address))
        }
        return result
    }

    private suspend fun fetchDetail(token: String, messageId: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$API_BASE/mailbox/${ProviderUtil.urlEncode(token)}/emails/${ProviderUtil.urlEncode(messageId)}",
                headers,
            )
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)?.get("data") as? JsonObject
        } catch (_: Exception) {
            null
        }
    }

    private fun buildRaw(raw: JsonObject, recipient: String): Map<String, Any?> {
        return mapOf(
            "id" to ProviderUtil.str(raw, "id"),
            "from" to ProviderUtil.firstNonEmpty(
                ProviderUtil.str(raw, "from"),
                ProviderUtil.str(raw, "from_address"),
                ProviderUtil.str(raw, "from_name"),
            ),
            "to" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "to"), recipient),
            "subject" to ProviderUtil.str(raw, "subject"),
            "text" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "text"), ProviderUtil.str(raw, "body_text")),
            "html" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "html"), ProviderUtil.str(raw, "body_html")),
            "date" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "date"), ProviderUtil.str(raw, "received_at")),
        )
    }
}
