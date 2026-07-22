package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * DevMail UK 渠道（devmail.uk）。
 */
object DevmailUk : Provider {

    private const val CHANNEL = "devmail-uk"
    private const val API_BASE = "https://www.devmail.uk/api"
    private val headers = mapOf("Accept" to "application/json")

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$API_BASE/new", headers)
        resp.ensureSuccess()
        val obj = ProviderUtil.parseObject(resp.body)
            ?: throw RuntimeException("devmail-uk: invalid new email response")
        var email = ProviderUtil.str(obj, "email").trim()
        if (email.isEmpty()) {
            val mailbox = ProviderUtil.str(obj, "mailbox").trim()
            if (mailbox.isNotEmpty()) email = "$mailbox@devmail.uk"
        }
        if (email.isEmpty() || !email.contains("@")) {
            throw RuntimeException("devmail-uk: invalid new email response")
        }
        return EmailInfo(email = email, channel = CHANNEL)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        val mailbox = if (address.contains("@")) address.substringBefore("@").trim() else address
        if (mailbox.isEmpty()) throw RuntimeException("devmail-uk: empty email")
        val resp = ProviderUtil.httpGet("$API_BASE/inbox/${ProviderUtil.urlEncode(mailbox)}?detail=true", headers)
        resp.ensureSuccess()
        val obj = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val rows = ProviderUtil.arr(obj, "emails") ?: return emptyList()
        return rows.filterIsInstance<JsonObject>().map { Normalize.fromJson(it, address) }
    }
}
