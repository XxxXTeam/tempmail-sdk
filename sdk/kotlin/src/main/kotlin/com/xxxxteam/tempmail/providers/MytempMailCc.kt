package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * MyTempMail.cc 渠道实现 — https://mytempmail.cc
 *
 * POST /api/address 创建邮箱，GET /api/mails/{token} 获取邮件。
 * token 为服务端返回的 API token。
 */
object MytempMailCc : Provider {

    private const val BASE_URL = "https://api.mytempmail.cc"
    private const val DEFAULT_DOMAIN = "nilvaro.com"

    private val HEADERS = mapOf(
        "Content-Type" to "application/json",
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 创建 mytempmail.cc 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val body = buildJsonObject {
            put("domain", DEFAULT_DOMAIN)
            put("name", ProviderUtil.randomString(10))
            put("expiry", 600)
        }.toString()
        val resp = ProviderUtil.httpPost("$BASE_URL/api/address", body, "application/json", HEADERS)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
            ?: throw RuntimeException("mytempmail-cc: 响应格式无效")
        val token = ProviderUtil.str(data, "token")
        val address = ProviderUtil.str(data, "address")
        if (token.isEmpty() || address.isEmpty() || !address.contains("@")) {
            throw RuntimeException("mytempmail-cc: 创建邮箱失败")
        }
        return EmailInfo(email = address, channel = "mytempmail-cc", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token.trim()
        if (token.isEmpty()) throw RuntimeException("mytempmail-cc: token 为空")
        val resp = ProviderUtil.httpGet("$BASE_URL/api/mails/${ProviderUtil.urlEncode(token)}", HEADERS)
        resp.ensureSuccess()
        val body = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val results = ProviderUtil.arr(body, "results") ?: return emptyList()
        val out = ArrayList<Email>(results.size)
        for (item in results) {
            val msg = item as? JsonObject ?: continue
            out.add(Normalize.fromJson(msg, info.email))
        }
        return out
    }
}
