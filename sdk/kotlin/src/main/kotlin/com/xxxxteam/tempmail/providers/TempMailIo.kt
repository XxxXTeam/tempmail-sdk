package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * temp-mail.io 渠道（api.internal.temp-mail.io/api/v3）。
 *
 * token 存创建时返回的 token（收信仅用邮箱地址）。
 */
object TempMailIo : Provider {

    private const val BASE_URL = "https://api.internal.temp-mail.io/api/v3"

    private fun buildHeaders() = mapOf(
        "Application-Name" to "web",
        "Application-Version" to "4.0.0",
        "X-CORS-Header" to "1",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
        "Origin" to "https://temp-mail.io",
        "Referer" to "https://temp-mail.io/",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val payload = buildJsonObject {
            put("min_name_length", 10)
            put("max_name_length", 10)
        }.toString()
        val resp = ProviderUtil.httpPost("$BASE_URL/email/new", payload, "application/json", buildHeaders())
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val email = ProviderUtil.str(data, "email")
        val token = ProviderUtil.str(data, "token")
        if (email.isEmpty() || token.isEmpty()) throw RuntimeException("temp-mail-io: invalid generate response")
        return EmailInfo(email = email, channel = "temp-mail-io", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val email = info.email
        val resp = ProviderUtil.httpGet(
            "$BASE_URL/email/" + ProviderUtil.urlEncode(email) + "/messages", buildHeaders())
        resp.ensureSuccess()
        val parsed = ProviderUtil.parse(resp.body) as? JsonArray ?: return emptyList()
        val result = ArrayList<Email>(parsed.size)
        for (raw in parsed) {
            val obj = raw as? JsonObject ?: continue
            result.add(Normalize.fromJson(obj, email))
        }
        return result
    }
}
