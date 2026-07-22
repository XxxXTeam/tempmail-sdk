package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * TempMail Fish 渠道 — https://tempmail.fish
 *
 * POST /emails/new-email 创建，GET /emails/emails?emailAddress={email} 收信。
 * token 存储 JSON: {"email":"...","authKey":"..."}
 */
object TempMailFish : Provider {

    private const val CHANNEL = "tempmail-fish"
    private const val API_BASE = "https://api.tempmail.fish"

    private val headers = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    )

    /** 创建临时邮箱，token 为 JSON 字符串 {"email":"...","authKey":"..."}。 */
    override suspend fun generate(): EmailInfo {
        val h = headers + ("Content-Type" to "application/json")
        val resp = ProviderUtil.httpPost("$API_BASE/emails/new-email", null, null, h)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val email = ProviderUtil.str(data, "email").trim()
        val authKey = ProviderUtil.str(data, "authKey").trim()
        if (email.isEmpty() || !email.contains("@") || authKey.isEmpty()) {
            throw RuntimeException("tempmail-fish: 创建邮箱响应无效")
        }
        val token = buildJsonObject {
            put("email", email)
            put("authKey", authKey)
        }.toString()
        return EmailInfo(email = email, channel = CHANNEL, token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        if (info.token.isBlank()) throw RuntimeException("tempmail-fish: token 为空")
        val tokenData = ProviderUtil.parseObject(info.token) ?: throw RuntimeException("tempmail-fish: token 格式无效")
        var address = ProviderUtil.str(tokenData, "email").trim()
        val authKey = ProviderUtil.str(tokenData, "authKey").trim()
        if (address.isEmpty() && info.email.isNotBlank()) address = info.email.trim()
        if (address.isEmpty() || authKey.isEmpty()) throw RuntimeException("tempmail-fish: token 缺少 email 或 authKey")

        val h = headers + ("Authorization" to authKey)
        val resp = ProviderUtil.httpGet("$API_BASE/emails/emails?emailAddress=${ProviderUtil.urlEncode(address)}", h)
        resp.ensureSuccess()
        val parsed = ProviderUtil.parse(resp.body)
        val rows: JsonArray = when (parsed) {
            is JsonArray -> parsed
            is JsonObject -> ProviderUtil.arr(parsed, "emails") ?: return emptyList()
            else -> return emptyList()
        }
        return rows.filterIsInstance<JsonObject>().map { item ->
            val to = ProviderUtil.str(item, "to")
            Normalize.fromMap(
                mapOf(
                    "from" to ProviderUtil.str(item, "from"),
                    "to" to to.ifEmpty { address },
                    "subject" to ProviderUtil.str(item, "subject"),
                    "text" to ProviderUtil.str(item, "textBody"),
                    "html" to ProviderUtil.str(item, "htmlBody"),
                    "date" to ProviderUtil.str(item, "timestamp"),
                ),
                address,
            )
        }
    }
}
