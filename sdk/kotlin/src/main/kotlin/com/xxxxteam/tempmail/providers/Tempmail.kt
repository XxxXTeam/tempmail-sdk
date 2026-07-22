package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * tempmail.ing 渠道实现。
 *
 * API: https://api.tempmail.ing/api
 */
object Tempmail : Provider {

    private const val CHANNEL = "tempmail"
    private const val BASE_URL = "https://api.tempmail.ing/api"
    private const val DEFAULT_DURATION = 60

    private val headers = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
        "Content-Type" to "application/json",
        "Referer" to "https://tempmail.ing/",
        "sec-ch-ua" to "\"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"",
        "sec-ch-ua-mobile" to "?0",
        "sec-ch-ua-platform" to "\"Windows\"",
        "DNT" to "1",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val payload = buildJsonObject { put("duration", DEFAULT_DURATION) }.toString()
        val resp = ProviderUtil.httpPost("$BASE_URL/generate", payload, "application/json", headers)
        resp.ensureSuccess()
        val root = ProviderUtil.parseObject(resp.body)
            ?: throw RuntimeException("tempmail: 无效的创建响应")
        if (!ProviderUtil.bool(root, "success")) throw RuntimeException("tempmail: 创建邮箱失败")
        val emailObj = root["email"] as? JsonObject ?: throw RuntimeException("tempmail: 响应缺少 email 对象")
        val address = ProviderUtil.str(emailObj, "address")
        if (address.isEmpty()) throw RuntimeException("tempmail: 响应缺少邮箱地址")
        return EmailInfo(
            email = address,
            channel = CHANNEL,
            expiresAt = ProviderUtil.str(emailObj, "expiresAt"),
            createdAt = ProviderUtil.str(emailObj, "createdAt"),
        )
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val resp = ProviderUtil.httpGet("$BASE_URL/emails/${ProviderUtil.urlEncode(info.email)}", headers)
        resp.ensureSuccess()
        val root = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        if (!ProviderUtil.bool(root, "success")) throw RuntimeException("tempmail: 获取邮件失败")
        val emails = ProviderUtil.arr(root, "emails") ?: return emptyList()
        return emails.filterIsInstance<JsonObject>().map { raw ->
            Normalize.fromMap(
                mapOf(
                    "id" to ProviderUtil.str(raw, "id"),
                    "from" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "from_address"), ProviderUtil.str(raw, "from")),
                    "to" to info.email,
                    "subject" to ProviderUtil.str(raw, "subject"),
                    "text" to ProviderUtil.str(raw, "text"),
                    "html" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "content"), ProviderUtil.str(raw, "html")),
                    "date" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "received_at"), ProviderUtil.str(raw, "date")),
                ),
                info.email,
            )
        }
    }
}
