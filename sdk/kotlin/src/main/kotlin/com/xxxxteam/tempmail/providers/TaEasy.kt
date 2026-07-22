package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * ta-easy.com 临时邮箱渠道。
 *
 * POST /temp-email/address/new 创建邮箱，POST /temp-email/inbox/list 获取邮件。
 */
object TaEasy : Provider {

    private const val CHANNEL = "ta-easy"
    private const val API_BASE = "https://api-endpoint.ta-easy.com"
    private const val ORIGIN = "https://www.ta-easy.com"

    private val headers = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Origin" to ORIGIN,
        "Referer" to "$ORIGIN/",
    )

    /** 创建 ta-easy 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val h = headers + ("Content-Length" to "0")
        val resp = ProviderUtil.httpPost("$API_BASE/temp-email/address/new", "", null, h)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val status = ProviderUtil.str(data, "status")
        val address = ProviderUtil.str(data, "address")
        val token = ProviderUtil.str(data, "token")
        if (status != "success" || address.isEmpty() || token.isEmpty()) {
            throw RuntimeException("ta-easy: 创建邮箱失败")
        }
        return EmailInfo(email = address, channel = CHANNEL, token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        val tok = info.token.trim()
        if (addr.isEmpty()) throw RuntimeException("ta-easy: 邮箱地址为空")
        if (tok.isEmpty()) throw RuntimeException("ta-easy: token 为空")
        val payload = buildJsonObject {
            put("token", tok)
            put("email", addr)
        }.toString()
        val h = headers + ("Content-Type" to "application/json")
        val resp = ProviderUtil.httpPost("$API_BASE/temp-email/inbox/list", payload, "application/json", h)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        if (ProviderUtil.str(data, "status") != "success") throw RuntimeException("ta-easy: 获取邮件失败")
        val arr = ProviderUtil.arr(data, "data") ?: return emptyList()
        return arr.filterIsInstance<JsonObject>().map { Normalize.fromJson(it, addr) }
    }
}
