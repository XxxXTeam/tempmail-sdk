package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * CleanTempMail 渠道（cleantempmail.com）。
 *
 * API Key 通过环境变量 CLEANTEMPMAIL_API_KEY 提供，缺省使用测试 key "ct-test"。
 */
object CleanTempMail : Provider {

    private const val CHANNEL = "cleantempmail"
    private const val API_BASE = "https://cleantempmail.com/api"

    private val headers: Map<String, String>
        get() {
            val apiKey = System.getenv("CLEANTEMPMAIL_API_KEY")?.takeIf { it.isNotBlank() }?.trim() ?: "ct-test"
            return mapOf(
                "Accept" to "application/json",
                "User-Agent" to "Mozilla/5.0",
                "X-API-Key" to apiKey,
            )
        }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$API_BASE/generate-email", headers)
        resp.ensureSuccess()
        val obj = ProviderUtil.parseObject(resp.body)
            ?: throw RuntimeException("cleantempmail: invalid generate-email response")
        val data = obj["data"] as? JsonObject
        val email = if (data != null) {
            ProviderUtil.firstNonEmpty(ProviderUtil.str(data, "email"), ProviderUtil.str(data, "mailbox"))
        } else ""
        if (email.isEmpty() || !email.contains("@")) {
            throw RuntimeException("cleantempmail: invalid generate-email response")
        }
        return EmailInfo(email = email, channel = CHANNEL)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("cleantempmail: empty email")
        val resp = ProviderUtil.httpGet("$API_BASE/emails?email=${ProviderUtil.urlEncode(address)}", headers)
        resp.ensureSuccess()
        val obj = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val data = obj["data"] as? JsonObject ?: return emptyList()
        val rows = ProviderUtil.arr(data, "emails") ?: return emptyList()
        return rows.filterIsInstance<JsonObject>().map { Normalize.fromJson(it, address) }
    }
}
