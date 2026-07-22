package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * tempmail.lol V2 渠道（POST /inbox/create）。使用服务端分配的域名。
 *
 * token 存创建时返回的会话令牌。
 */
object TempMailLol : Provider {

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val payload = buildJsonObject {
            put("domain", JsonNull)
            put("captcha", JsonNull)
        }.toString()
        val resp = ProviderUtil.httpPost("$BASE_URL/inbox/create", payload, "application/json", HEADERS)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val address = ProviderUtil.str(data, "address")
        val token = ProviderUtil.str(data, "token")
        if (address.isEmpty() || token.isEmpty()) throw RuntimeException("Failed to generate email")
        return EmailInfo(email = address, channel = "tempmail-lol", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val email = info.email
        val resp = ProviderUtil.httpGet(
            "$BASE_URL/inbox?token=" + ProviderUtil.urlEncode(info.token), HEADERS)
        resp.ensureSuccess()
        val emails = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "emails") ?: return emptyList()
        val result = ArrayList<Email>(emails.size)
        for (raw in emails) {
            val obj = raw as? JsonObject ?: continue
            result.add(Normalize.fromJson(obj, email))
        }
        return result
    }

    private const val BASE_URL = "https://api.tempmail.lol/v2"
    private val HEADERS = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
        "Origin" to "https://tempmail.lol",
    )
}
