package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * TempGo Email 渠道（tempgo.email）。
 *
 * POST /api/generate 创建，token 为 mailbox_id；
 * GET /api/inbox?email={}&mailbox_id={} 收信。
 */
object TempGoEmail : Provider {

    private const val CHANNEL = "tempgo-email"
    private const val BASE_URL = "https://tempgo.email"
    private val headers = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        "Accept" to "application/json",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$BASE_URL/api/generate", null, null, headers)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val email = ProviderUtil.str(data, "email").trim()
        val mailboxId = ProviderUtil.str(data, "mailbox_id").trim()
        if (email.isEmpty() || mailboxId.isEmpty()) throw RuntimeException("tempgo-email: 创建邮箱响应无效")
        return EmailInfo(email = email, channel = CHANNEL, token = mailboxId)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val mailboxId = info.token.trim()
        val address = info.email.trim()
        if (mailboxId.isEmpty()) throw RuntimeException("tempgo-email: token 为空")
        if (address.isEmpty()) throw RuntimeException("tempgo-email: 邮箱地址为空")
        val url = "$BASE_URL/api/inbox?email=" + ProviderUtil.urlEncode(address) +
            "&mailbox_id=" + ProviderUtil.urlEncode(mailboxId)
        val resp = ProviderUtil.httpGet(url, headers)
        if (resp.statusCode == 404) return emptyList()
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val messages = ProviderUtil.arr(data, "messages") ?: return emptyList()
        return messages.filterIsInstance<JsonObject>().map { item ->
            // putIfAbsent 语义：原字段优先，缺失时回退别名字段
            val row = mapOf(
                "id" to ProviderUtil.str(item, "id"),
                "to" to address,
                "from" to ProviderUtil.firstNonEmpty(ProviderUtil.str(item, "from"), ProviderUtil.str(item, "sender")),
                "subject" to ProviderUtil.str(item, "subject"),
                "text" to ProviderUtil.firstNonEmpty(ProviderUtil.str(item, "text"), ProviderUtil.str(item, "body_plain")),
                "html" to ProviderUtil.firstNonEmpty(ProviderUtil.str(item, "html"), ProviderUtil.str(item, "body_html")),
                "date" to ProviderUtil.firstNonEmpty(ProviderUtil.str(item, "date"), ProviderUtil.str(item, "received_at")),
            )
            Normalize.fromMap(row, address)
        }
    }
}
