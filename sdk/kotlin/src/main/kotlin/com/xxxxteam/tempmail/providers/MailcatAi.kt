package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * mailcat.ai 渠道（mailcat.ai）。
 *
 * POST /mailboxes 创建邮箱（无 body），返回 Bearer token；
 * GET /inbox 获取邮件列表。
 */
object MailcatAi : Provider {

    private const val CHANNEL = "mailcat-ai"
    private const val BASE_URL = "https://api.mailcat.ai"
    private val headers = mapOf(
        "User-Agent" to "Mozilla/5.0",
        "Accept" to "application/json",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$BASE_URL/mailboxes", null, null, headers)
        resp.ensureSuccess()
        val body = ProviderUtil.parseObject(resp.body) ?: throw RuntimeException("mailcat-ai: 响应格式无效")
        val data = body["data"] as? JsonObject ?: throw RuntimeException("mailcat-ai: 响应 data 格式无效")
        val address = ProviderUtil.str(data, "email")
        val token = ProviderUtil.str(data, "token")
        if (address.isEmpty()) throw RuntimeException("mailcat-ai: 缺少邮箱地址")
        if (token.isEmpty()) throw RuntimeException("mailcat-ai: 缺少认证令牌")
        return EmailInfo(email = address, channel = CHANNEL, token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val h = headers + ("Authorization" to "Bearer ${info.token}")
        val resp = ProviderUtil.httpGet("$BASE_URL/inbox", h)
        if (resp.statusCode == 404) return emptyList()
        resp.ensureSuccess()
        val body = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val messages = ProviderUtil.arr(body, "data") ?: return emptyList()
        return messages.filterIsInstance<JsonObject>().map { Normalize.fromJson(it, info.email) }
    }
}
