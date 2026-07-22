package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * DropMail.click 渠道（dropmail.click）。
 *
 * 独立后端临时邮箱，免注册、无鉴权。token 即邮箱地址本身。
 */
object DropmailClick : Provider {

    private const val CHANNEL = "dropmail-click"
    private const val BASE_URL = "https://dropmail.click"
    private val headers = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        "Accept" to "application/json",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$BASE_URL/api/v1/public/mailbox", null, "application/json", headers)
        resp.ensureSuccess()
        val root = ProviderUtil.parseObject(resp.body) ?: throw RuntimeException("dropmail-click: 无效响应")
        val address = ProviderUtil.str(root, "address").trim()
        if (address.isEmpty()) throw RuntimeException("dropmail-click: 无效响应，缺少 address")
        return EmailInfo(email = address, channel = CHANNEL, token = address)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.token.ifBlank { info.email }.trim()
        if (addr.isEmpty()) throw RuntimeException("dropmail-click: 缺少邮箱地址")
        val resp = ProviderUtil.httpGet("$BASE_URL/api/v1/public/mailbox/" + ProviderUtil.urlEncode(addr), headers)
        resp.ensureSuccess()
        val root = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val msgs = ProviderUtil.arr(root, "messages") ?: return emptyList()
        return msgs.filterIsInstance<JsonObject>().map { item ->
            val row = mapOf(
                "id" to ProviderUtil.str(item, "id"),
                "from" to ProviderUtil.str(item, "from"),
                "to" to ProviderUtil.firstNonEmpty(ProviderUtil.str(item, "address"), addr),
                "subject" to ProviderUtil.str(item, "subject"),
                "text" to ProviderUtil.str(item, "text"),
                "html" to ProviderUtil.str(item, "html"),
                "date" to ProviderUtil.firstNonEmpty(ProviderUtil.str(item, "received_at"), ProviderUtil.str(item, "date")),
            )
            Normalize.fromMap(row, addr)
        }
    }
}
