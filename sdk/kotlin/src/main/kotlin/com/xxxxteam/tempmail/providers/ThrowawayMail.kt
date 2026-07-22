package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * ThrowawayMail 渠道 — https://throwawaymail.app
 *
 * POST /api/mailboxes 创建，GET /api/mailboxes/{id}/messages 收信。
 */
object ThrowawayMail : Provider {

    private const val CHANNEL = "throwawaymail"
    private const val API_BASE = "https://throwawaymail.app/api"
    private val headers = mapOf("Accept" to "application/json")

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$API_BASE/mailboxes", null, null, headers)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val mailboxId = ProviderUtil.str(data, "mailbox_id").trim()
        val email = ProviderUtil.str(data, "address").trim()
        if (mailboxId.isEmpty() || email.isEmpty() || !email.contains("@")) {
            throw RuntimeException("throwawaymail: 创建邮箱响应无效")
        }
        return EmailInfo(email = email, channel = CHANNEL, token = mailboxId)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val mailboxId = info.token.trim()
        val address = info.email.trim()
        if (mailboxId.isEmpty()) throw RuntimeException("throwawaymail: mailbox_id 为空")
        if (address.isEmpty()) throw RuntimeException("throwawaymail: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$API_BASE/mailboxes/${ProviderUtil.urlEncode(mailboxId)}/messages", headers)
        resp.ensureSuccess()
        val arr = ProviderUtil.parse(resp.body) as? JsonArray ?: return emptyList()
        val result = mutableListOf<Email>()
        for (item in arr.filterIsInstance<JsonObject>()) {
            val msgId = ProviderUtil.str(item, "message_id").trim()
            val detail = if (msgId.isNotEmpty()) fetchDetail(mailboxId, msgId) else null
            result.add(Normalize.fromJson(detail ?: item, address))
        }
        return result
    }

    private suspend fun fetchDetail(mailboxId: String, messageId: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$API_BASE/mailboxes/${ProviderUtil.urlEncode(mailboxId)}/messages/${ProviderUtil.urlEncode(messageId)}",
                headers,
            )
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }
}
