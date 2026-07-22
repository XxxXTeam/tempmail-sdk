package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * TempInbox 渠道（tempinbox.xyz）。
 *
 * GET /email/Random 获取随机邮箱，GET /messages/{email} 收信。
 */
object Tempinbox : Provider {

    private const val BASE = "https://endpoint.tempinbox.xyz"
    private val HEADERS = mapOf(
        "Accept" to "application/json, text/plain, */*",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer" to "https://www.tempinbox.xyz/",
        "sec-ch-ua" to "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        "sec-ch-ua-mobile" to "?0",
        "sec-ch-ua-platform" to "\"Windows\"",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    /** 创建临时邮箱（使用随机邮箱端点）。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE/email/Random", HEADERS)
        resp.ensureSuccess()
        val addr = resp.body.trim().replace("\"", "")
        if (addr.isBlank() || !addr.contains("@")) throw RuntimeException("tempinbox: 无效的邮箱地址")
        return EmailInfo(email = addr, channel = "tempinbox")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        if (e.isBlank()) throw RuntimeException("tempinbox: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE/messages/$e", HEADERS)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, e) }
    }
}
