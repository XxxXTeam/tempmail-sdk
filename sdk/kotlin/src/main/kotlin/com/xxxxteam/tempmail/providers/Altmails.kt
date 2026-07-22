package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Altmails 渠道（tempmail.altmails.com）。
 *
 * GET / 取 session 和 CSRF，GET /random-email-address 取随机邮箱，
 * POST /fetch-emails/{email} 收信，GET /view/{id} 取正文。token 存储 JSON {csrf, cookies}。
 */
object Altmails : Provider {

    private const val BASE_URL = "https://tempmail.altmails.com"
    private val CSRF_RE = Regex("['\"]_token['\"]\\s*:\\s*['\"]([^'\"]+)['\"]")

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val r1 = ProviderUtil.httpGet("$BASE_URL/",
            mapOf("Accept" to "text/html,application/xhtml+xml", "User-Agent" to "Mozilla/5.0"))
        r1.ensureSuccess()
        var cookies = cookieStr(r1)
        val csrf = CSRF_RE.find(r1.body)?.groupValues?.get(1)
            ?: throw RuntimeException("altmails: 无法从首页提取 CSRF token")
        val h2 = mapOf(
            "Accept" to "text/html", "User-Agent" to "Mozilla/5.0",
            "Cookie" to cookies, "Referer" to "$BASE_URL/",
        )
        val r2 = ProviderUtil.httpGet("$BASE_URL/random-email-address", h2)
        r2.ensureSuccess()
        cookies = mergeCookieStr(cookies, r2)
        val mailbox = r2.body.trim()
        if (mailbox.isBlank() || !mailbox.contains("@")) {
            throw RuntimeException("altmails: 返回的邮箱地址无效")
        }
        val token = buildJsonObject { put("csrf", csrf); put("cookies", cookies) }.toString()
        return EmailInfo(email = mailbox, channel = "altmails", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val email = info.email.trim()
        if (email.isBlank()) throw RuntimeException("altmails: 邮箱地址为空")
        val tokObj = Http.json.parseToJsonElement(info.token) as? JsonObject
            ?: throw RuntimeException("altmails: token 格式无效")
        val csrf = jsonStr(tokObj, "csrf")
        val cookies = jsonStr(tokObj, "cookies")
        val h = mapOf(
            "Accept" to "application/json",
            "X-Requested-With" to "XMLHttpRequest",
            "Content-Type" to "application/x-www-form-urlencoded",
            "Cookie" to cookies, "Referer" to "$BASE_URL/", "User-Agent" to "Mozilla/5.0",
        )
        val resp = ProviderUtil.httpPost("$BASE_URL/fetch-emails/$email",
            "_token=${ProviderUtil.urlEncode(csrf)}", "application/x-www-form-urlencoded", h)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { msg ->
            val mailId = jsonStr(msg, "id")
            val htmlBody = if (mailId.isNotEmpty()) fetchView(mailId, cookies) else ""
            val flat = mapOf(
                "id" to mailId,
                "from" to jsonStr(msg, "from"),
                "subject" to jsonStr(msg, "subject"),
                "html" to htmlBody,
                "to" to email,
                "date" to jsonStr(msg, "date"),
            )
            Normalize.fromMap(flat, email)
        }
    }

    /** 拉取单封邮件 HTML 正文，失败返回空串。 */
    private suspend fun fetchView(mailId: String, cookies: String): String {
        return try {
            val h = mapOf("User-Agent" to "Mozilla/5.0", "Cookie" to cookies, "Referer" to "$BASE_URL/")
            val vr = ProviderUtil.httpGet("$BASE_URL/view/$mailId", h)
            if (vr.isOk) vr.body else ""
        } catch (_: Exception) {
            ""
        }
    }

    /** 从响应 Set-Cookie 拼接 name=value; 串。 */
    private fun cookieStr(resp: HttpResp): String =
        resp.setCookies.joinToString("; ") { it.split(";")[0].trim() }

    /** 合并已有 cookie 串与响应新 cookie。 */
    private fun mergeCookieStr(prev: String, resp: HttpResp): String {
        val jar = LinkedHashMap<String, String>()
        prev.split(";").forEach { part ->
            val p = part.trim()
            val eq = p.indexOf('=')
            if (eq > 0) jar[p.substring(0, eq).trim()] = p.substring(eq + 1).trim()
        }
        resp.setCookies.forEach { sc ->
            val seg = sc.split(";")[0].trim()
            val eq = seg.indexOf('=')
            if (eq > 0) jar[seg.substring(0, eq)] = seg.substring(eq + 1)
        }
        return jar.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
