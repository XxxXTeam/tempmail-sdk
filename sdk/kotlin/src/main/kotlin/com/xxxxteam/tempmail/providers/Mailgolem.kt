package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Mailgolem 渠道（mailgolem.com）。
 *
 * GET / 取 session cookie 和 CSRF，GET /random-email-address 取邮箱，
 * POST /fetch-emails/{email} 收信。token 存储 JSON {csrf, cookies}。
 */
object Mailgolem : Provider {

    private const val BASE = "https://mailgolem.com"
    private val CSRF_RE = Regex("<input[^>]+name=\"_token\"[^>]+value=\"([^\"]+)\"")

    private fun defaultHeaders() = mapOf(
        "Accept" to "text/html,application/xhtml+xml",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val r1 = ProviderUtil.httpGet("$BASE/", defaultHeaders())
        r1.ensureSuccess()
        var cookies = cookieStr(r1)
        val csrf = CSRF_RE.find(r1.body)?.groupValues?.get(1)
            ?: throw RuntimeException("mailgolem: 无法提取 CSRF token")
        val h2 = defaultHeaders() + mapOf("Referer" to "$BASE/", "Cookie" to cookies)
        val r2 = ProviderUtil.httpGet("$BASE/random-email-address", h2)
        r2.ensureSuccess()
        val more = cookieStr(r2)
        if (more.isNotEmpty()) cookies = if (cookies.isEmpty()) more else "$cookies; $more"
        val emailAddr = r2.body.trim()
        if (emailAddr.isBlank() || !emailAddr.contains("@")) {
            throw RuntimeException("mailgolem: 获取到无效的邮箱地址")
        }
        val token = buildJsonObject { put("csrf", csrf); put("cookies", cookies) }.toString()
        return EmailInfo(email = emailAddr, channel = "mailgolem", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        if (addr.isBlank()) throw RuntimeException("mailgolem: 邮箱地址为空")
        // 重新获取 session（原 session 可能已过期）
        val r1 = ProviderUtil.httpGet("$BASE/", defaultHeaders())
        r1.ensureSuccess()
        val cookies = cookieStr(r1)
        val newCsrf = CSRF_RE.find(r1.body)?.groupValues?.get(1)
            ?: throw RuntimeException("mailgolem: 无法提取 CSRF token")
        val h = mapOf(
            "Accept" to "application/json, text/plain, */*",
            "Content-Type" to "application/x-www-form-urlencoded",
            "X-Requested-With" to "XMLHttpRequest",
            "Referer" to "$BASE/",
            "User-Agent" to "Mozilla/5.0",
            "Cookie" to cookies,
        )
        val r2 = ProviderUtil.httpPost("$BASE/fetch-emails/$addr",
            "_token=${ProviderUtil.urlEncode(newCsrf)}", "application/x-www-form-urlencoded", h)
        r2.ensureSuccess()
        val arr = Http.json.parseToJsonElement(r2.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { msg ->
            val flat = mapOf(
                "id" to jsonStr(msg, "id"),
                "from" to jsonStr(msg, "from"),
                "to" to addr,
                "subject" to jsonStr(msg, "subject"),
            )
            Normalize.fromMap(flat, addr)
        }
    }

    /** 从响应 Set-Cookie 拼接 name=value; 串。 */
    private fun cookieStr(resp: HttpResp): String =
        resp.setCookies.joinToString("; ") { it.split(";")[0].trim() }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
