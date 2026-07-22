package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Gonebox Email 渠道（gonebox.email）。
 *
 * 一次性临时邮箱服务，无需认证。POST /inboxes 创建，
 * GET /inboxes/{email}/messages 收信。
 */
object GoneboxEmail : Provider {

    private const val CHANNEL = "gonebox-email"
    private const val BASE_URL = "https://api.gonebox.email/api/v1"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
    private val postHeaders = mapOf("User-Agent" to UA, "Accept" to "application/json", "Content-Type" to "application/json")
    private val getHeaders = mapOf("User-Agent" to UA, "Accept" to "application/json")

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val body = buildJsonObject { put("domain", "gonebox.email") }.toString()
        val resp = ProviderUtil.httpPost("$BASE_URL/inboxes", body, "application/json", postHeaders)
        resp.ensureSuccess()
        val root = ProviderUtil.parseObject(resp.body) ?: throw RuntimeException("gonebox-email: 创建邮箱失败")
        if (!ProviderUtil.bool(root, "success")) throw RuntimeException("gonebox-email: 创建邮箱失败")
        val data = root["data"] as? JsonObject ?: throw RuntimeException("gonebox-email: 响应 data 格式无效")
        val address = ProviderUtil.str(data, "address").trim()
        if (address.isEmpty()) throw RuntimeException("gonebox-email: 缺少邮箱地址")
        // 过期时间为秒级时间戳，转毫秒
        val expiresAt = ProviderUtil.str(data, "expiresAt").trim()
            .toLongOrNull()?.let { (it * 1000).toString() } ?: ""
        return EmailInfo(email = address, channel = CHANNEL, expiresAt = expiresAt)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("gonebox-email: 缺少邮箱地址")
        val resp = ProviderUtil.httpGet("$BASE_URL/inboxes/" + ProviderUtil.urlEncode(address) + "/messages", getHeaders)
        if (resp.statusCode == 404) return emptyList()
        resp.ensureSuccess()
        val root = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        if (!ProviderUtil.bool(root, "success")) return emptyList()
        val data = root["data"] as? JsonObject ?: return emptyList()
        val msgs = ProviderUtil.arr(data, "messages") ?: return emptyList()
        return msgs.filterIsInstance<JsonObject>().map { msg ->
            val row = mapOf(
                "id" to ProviderUtil.str(msg, "id"),
                "from" to ProviderUtil.firstNonEmpty(ProviderUtil.str(msg, "from"), ProviderUtil.str(msg, "sender")),
                "to" to address,
                "subject" to ProviderUtil.str(msg, "subject"),
                "text" to ProviderUtil.firstNonEmpty(ProviderUtil.str(msg, "text"), ProviderUtil.str(msg, "body_plain")),
                "html" to ProviderUtil.firstNonEmpty(ProviderUtil.str(msg, "html"), ProviderUtil.str(msg, "body_html")),
                "date" to ProviderUtil.firstNonEmpty(ProviderUtil.str(msg, "date"), ProviderUtil.str(msg, "received_at")),
            )
            Normalize.fromMap(row, address)
        }
    }
}
