package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * linshiyouxiang.net 渠道（www.linshiyouxiang.net）。
 *
 * GET / 从 HTML 提取 tempMailGlobal（邮箱）和 mailCodeGlobal（校验码）。
 * POST /get-messages 收信。code 存入 EmailInfo.token。
 */
object LinshiyouxiangNet : Provider {

    private const val BASE_URL = "https://www.linshiyouxiang.net"
    private val EMAIL_RE = Regex("tempMailGlobal\\s*=\\s*'([^']+)'")
    private val CODE_RE = Regex("mailCodeGlobal\\s*=\\s*'([^']+)'")

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val h = mapOf("User-Agent" to "Mozilla/5.0", "Accept" to "text/html")
        val resp = ProviderUtil.httpGet("$BASE_URL/", h)
        resp.ensureSuccess()
        val html = resp.body
        val email = EMAIL_RE.find(html)?.groupValues?.get(1)?.trim()
            ?: throw RuntimeException("linshiyouxiang-net: 未能提取邮箱地址")
        val code = CODE_RE.find(html)?.groupValues?.get(1)?.trim() ?: ""
        return EmailInfo(email = email, channel = "linshiyouxiang-net", token = code)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        if (e.isBlank()) throw RuntimeException("linshiyouxiang-net: 邮箱地址为空")
        val h = mapOf(
            "Accept" to "application/json",
            "Content-Type" to "application/json",
            "Referer" to "$BASE_URL/",
            "Origin" to BASE_URL,
            "User-Agent" to "Mozilla/5.0",
            "X-Requested-With" to "XMLHttpRequest",
        )
        val payload = buildJsonObject {
            put("email", e)
            put("code", info.token)
        }.toString()
        val resp = ProviderUtil.httpPost("$BASE_URL/get-messages", payload, "application/json", h)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val emails = root["emails"] as? JsonArray ?: return emptyList()
        return emails.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, e) }
    }
}
