package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Byom 渠道（byom.de）。
 *
 * 纯 GET 无认证，本地直接生成随机用户名 + @byom.de。
 */
object Byom : Provider {

    private const val DOMAIN = "byom.de"
    private const val BASE = "https://api.byom.de"
    private val HEADERS = mapOf(
        "Accept" to "application/json, text/plain, */*",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    /** 创建临时邮箱（本地生成，无需 API 调用）。 */
    override suspend fun generate(): EmailInfo {
        val username = ProviderUtil.randomString(10)
        return EmailInfo(email = "$username@$DOMAIN", channel = "byom")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        if (e.isBlank()) throw RuntimeException("byom: 邮箱地址为空")
        val username = e.substringBefore("@")
        if (username.isBlank()) throw RuntimeException("byom: 邮箱格式无效")
        val resp = ProviderUtil.httpGet("$BASE/mails/${ProviderUtil.urlEncode(username)}", HEADERS)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, e) }
    }
}
