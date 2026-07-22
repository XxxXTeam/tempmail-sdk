package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Mail Sunls 渠道（mail.sunls.de）。
 *
 * 无 token 无 session。从 /api/domain 获取域名列表，本地随机生成地址。
 */
object MailSunls : Provider {

    private const val BASE = "https://mail.sunls.de"
    private val HEADERS = mapOf(
        "Accept" to "application/json, text/plain, */*",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer" to "https://mail.sunls.de/",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val domains = fetchDomains()
        val domain = domains.random()
        val local = ProviderUtil.randomString(10)
        return EmailInfo(email = "$local@$domain", channel = "mail-sunls")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        if (addr.isBlank()) throw RuntimeException("mail-sunls: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE/api/fetch?to=${ProviderUtil.urlEncode(addr)}", HEADERS)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, addr) }
    }

    /** 拉取可用域名列表。 */
    private suspend fun fetchDomains(): List<String> {
        val resp = ProviderUtil.httpGet("$BASE/api/domain", HEADERS)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray
            ?: throw RuntimeException("mail-sunls: 域名列表响应格式无效")
        val out = arr.mapNotNull { (it as? JsonPrimitive)?.contentOrNull?.trim() }
            .filter { it.isNotEmpty() }
        if (out.isEmpty()) throw RuntimeException("mail-sunls: 无可用域名")
        return out
    }
}
