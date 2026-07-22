package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * MailForSpam 渠道（mailforspam.com）。
 *
 * 域名别名：mailforspam.com / tempmail.io / disposable.email
 *
 * @property channel 对外渠道标识
 * @property domain 邮箱域名
 */
class MailForSpam(private val channel: String, private val domain: String) : Provider {

    private val headers = mapOf(
        "Accept" to "application/json",
        "Referer" to "https://mailforspam.com/",
        "Origin" to "https://mailforspam.com",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val selected = pickDomain(domain)
        val email = "${ProviderUtil.randomLocalSdk()}@$selected"
        val resp = ProviderUtil.httpGet(mailboxUrl(email, 1), headers)
        resp.ensureSuccess()
        return EmailInfo(email = email, channel = channel)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("mailforspam: empty email")
        val resp = ProviderUtil.httpGet(mailboxUrl(address, 100), headers)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val rows = ProviderUtil.arr(data, "emails") ?: return emptyList()
        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val messageId = ProviderUtil.str(item, "id").trim()
            if (messageId.isEmpty()) continue
            val detail = fetchMessage(messageId, address)
            val source = detail ?: item
            result.add(Normalize.fromMap(flatten(source, address), address))
        }
        return result
    }

    private fun mailboxUrl(email: String, pageSize: Int): String =
        "$API_BASE/mailboxes/${ProviderUtil.urlEncode(email)}/emails?page=1&page_size=$pageSize&sort_by=date&sort_order=desc"

    private suspend fun fetchMessage(messageId: String, email: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$API_BASE/mailboxes/${ProviderUtil.urlEncode(email)}/emails/${ProviderUtil.urlEncode(messageId)}",
                headers,
            )
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }

    private fun flatten(raw: JsonObject, recipient: String): Map<String, Any?> {
        val receiver = ProviderUtil.str(raw, "receiver")
        val readAt = ProviderUtil.str(raw, "readAt")
        return mapOf(
            "id" to ProviderUtil.str(raw, "id"),
            "from" to ProviderUtil.str(raw, "sender"),
            "to" to receiver.ifEmpty { recipient },
            "subject" to ProviderUtil.str(raw, "subject"),
            "text" to ProviderUtil.str(raw, "body_text"),
            "html" to ProviderUtil.str(raw, "body_html"),
            "date" to ProviderUtil.str(raw, "date"),
            "isRead" to readAt.isNotEmpty(),
        )
    }

    private fun pickDomain(preferred: String?): String {
        var p = (preferred ?: "").trim()
        if (p.startsWith("@")) p = p.substring(1)
        p = p.lowercase()
        if (p.isNotEmpty() && p in DOMAINS) return p
        return DEFAULT_DOMAIN
    }

    companion object {
        private const val API_BASE = "https://api.mailforspam.com/api"
        private const val DEFAULT_DOMAIN = "mailforspam.com"
        private val DOMAINS = setOf("mailforspam.com", "tempmail.io", "disposable.email")
    }
}
