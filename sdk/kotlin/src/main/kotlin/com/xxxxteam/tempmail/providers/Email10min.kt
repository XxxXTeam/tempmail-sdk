package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.nio.charset.StandardCharsets
import java.util.Base64

/**
 * email10min 渠道（email10min.com）。
 *
 * GET /zh 获取 CSRF + Cookie + 邮箱地址，POST /messages 获取邮件。
 * token 使用 base64 编码，前缀 "e10m:"，内含 cookie 和 csrf。
 */
object Email10min : Provider {

    private const val BASE_URL = "https://email10min.com"
    private const val TOK_PREFIX = "e10m:"
    private val CSRF_META_RE = Regex("(?i)<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"")
    private val EMAIL_ID_RE = Regex("(?i)id=\"emailAddress\"[^>]*>([^<]+)")
    private val EMAIL_DATA_RE = Regex("(?i)data-email=\"([^\"]+@[^\"]+)\"")
    private val EMAIL_JSON_RE = Regex("\"mailbox\"\\s*:\\s*\"([^\"]+@[^\"]+)\"")
    private val EMAIL_GENERIC_RE = Regex("([a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,})")

    /** 拼接响应中的 Set-Cookie 为 Cookie 头。 */
    private fun cookieFromResp(setCookies: List<String>): String =
        setCookies.joinToString("; ") { it.split(";")[0].trim() }

    /** 编码 token。 */
    private fun encodeToken(cookie: String, csrf: String): String {
        val raw = buildJsonObject {
            put("c", cookie)
            put("t", csrf)
        }.toString()
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.toByteArray(StandardCharsets.UTF_8))
    }

    /** 解码 token，返回 (cookie, csrf)。 */
    private fun decodeToken(token: String): Pair<String, String> {
        if (!token.startsWith(TOK_PREFIX)) throw RuntimeException("email10min: 无效的 session token")
        val raw = String(Base64.getDecoder().decode(token.substring(TOK_PREFIX.length)), StandardCharsets.UTF_8)
        val obj = Http.json.parseToJsonElement(raw) as? JsonObject
            ?: throw RuntimeException("email10min: 无效的 session token")
        val cookie = jsonStr(obj, "c").trim()
        val csrf = jsonStr(obj, "t").trim()
        if (cookie.isEmpty() || csrf.isEmpty()) {
            throw RuntimeException("email10min: 无效的 session token (字段为空)")
        }
        return cookie to csrf
    }

    /** 从 HTML 中提取邮箱地址。 */
    private fun extractEmail(html: String): String {
        EMAIL_ID_RE.find(html)?.groupValues?.get(1)?.let { if (it.contains("@")) return it.trim() }
        EMAIL_DATA_RE.find(html)?.groupValues?.get(1)?.let { return it.trim() }
        EMAIL_JSON_RE.find(html)?.groupValues?.get(1)?.let { return it.trim() }
        EMAIL_GENERIC_RE.find(html)?.groupValues?.get(1)?.let { addr ->
            if (!addr.contains("email10min") && !addr.contains("example") && !addr.contains("google")) {
                return addr.trim()
            }
        }
        throw RuntimeException("email10min: 未找到邮箱地址")
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val h = mapOf("Accept" to "text/html", "User-Agent" to "Mozilla/5.0")
        val resp = ProviderUtil.httpGet("$BASE_URL/zh", h)
        resp.ensureSuccess()
        val cookie = cookieFromResp(resp.setCookies)
        val html = resp.body
        val csrf = CSRF_META_RE.find(html)?.groupValues?.get(1)
            ?: throw RuntimeException("email10min: 未找到 CSRF token")
        val emailAddr = extractEmail(html)
        return EmailInfo(email = emailAddr, channel = "email10min", token = encodeToken(cookie, csrf))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val (cookie, csrf) = decodeToken(info.token)
        val h = mapOf(
            "Accept" to "application/json, text/plain, */*",
            "Content-Type" to "application/x-www-form-urlencoded; charset=UTF-8",
            "Origin" to BASE_URL,
            "Referer" to "$BASE_URL/zh",
            "User-Agent" to "Mozilla/5.0",
            "X-Requested-With" to "XMLHttpRequest",
            "Cookie" to cookie,
        )
        val ts = System.currentTimeMillis().toString()
        val resp = ProviderUtil.httpPost(
            "$BASE_URL/messages?$ts",
            "_token=${ProviderUtil.urlEncode(csrf)}&captcha=",
            "application/x-www-form-urlencoded; charset=UTF-8", h,
        )
        resp.ensureSuccess()
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val messages = body["messages"] as? JsonArray ?: return emptyList()
        return messages.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, info.email) }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
