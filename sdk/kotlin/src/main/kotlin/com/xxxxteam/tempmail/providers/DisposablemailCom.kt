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
 * disposablemail.com 渠道（www.disposablemail.com）。
 *
 * GET / 取 session cookie + CSRF，GET /index/index 创建邮箱，
 * GET /index/refresh 收信，GET /email/id/{id} 取正文。token base64 编码，前缀 "dmc1:"。
 */
object DisposablemailCom : Provider {

    private const val BASE_URL = "https://www.disposablemail.com"
    private const val TOK_PREFIX = "dmc1:"
    private val CSRF_RE = Regex("CSRF=\"([^\"]+)\"")

    private fun ajaxHeaders(cookie: String): Map<String, String> {
        val h = mutableMapOf(
            "User-Agent" to "Mozilla/5.0",
            "Accept" to "application/json, text/javascript, */*; q=0.01",
            "X-Requested-With" to "XMLHttpRequest",
            "Referer" to "$BASE_URL/",
        )
        if (cookie.isNotEmpty()) h["Cookie"] = cookie
        return h
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val r1 = ProviderUtil.httpGet("$BASE_URL/",
            mapOf("User-Agent" to "Mozilla/5.0", "Accept" to "text/html"))
        r1.ensureSuccess()
        var cookie = mergeCookies("", r1)
        val csrf = CSRF_RE.find(r1.body)?.groupValues?.get(1)
            ?: throw RuntimeException("disposablemail-com: 未能提取 CSRF token")
        val r2 = ProviderUtil.httpGet(
            "$BASE_URL/index/index?csrf_token=${ProviderUtil.urlEncode(csrf)}", ajaxHeaders(cookie))
        r2.ensureSuccess()
        cookie = mergeCookies(cookie, r2)
        val data = Http.json.parseToJsonElement(r2.body) as? JsonObject
            ?: throw RuntimeException("disposablemail-com: 创建邮箱响应格式异常")
        val email = jsonStr(data, "email").trim()
        if (email.isBlank() || !email.contains("@")) throw RuntimeException("disposablemail-com: 邮箱地址无效")
        return EmailInfo(email = email, channel = "disposablemail-com", token = encodeToken(csrf, cookie))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val email = info.email.trim()
        val cookie = decodeToken(info.token)
        val resp = ProviderUtil.httpGet("$BASE_URL/index/refresh", ajaxHeaders(cookie))
        resp.ensureSuccess()
        val trimmed = resp.body.trim()
        if (trimmed == "0" || trimmed.isEmpty() || trimmed == "[]") return emptyList()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { msg ->
            val mailId = jsonStr(msg, "id")
            val flat = mapOf(
                "id" to mailId,
                "from" to jsonStr(msg, "od"),
                "to" to email,
                "subject" to jsonStr(msg, "predmet"),
                "html" to fetchBody(mailId, cookie),
                "date" to jsonStr(msg, "kdy"),
            )
            Normalize.fromMap(flat, email)
        }
    }

    /** 拉取单封邮件正文，失败返回空串。 */
    private suspend fun fetchBody(mailId: String, cookie: String): String {
        if (mailId.isEmpty()) return ""
        return try {
            val resp = ProviderUtil.httpGet("$BASE_URL/email/id/$mailId", ajaxHeaders(cookie))
            if (resp.isOk) resp.body else ""
        } catch (_: Exception) {
            ""
        }
    }

    /** 编码 token（base64，前缀 dmc1:），存储 {t:csrf, c:cookie}。 */
    private fun encodeToken(csrf: String, cookie: String): String {
        val raw = buildJsonObject { put("t", csrf); put("c", cookie) }.toString()
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.toByteArray(StandardCharsets.UTF_8))
    }

    /** 解码 token，返回 cookie 串。 */
    private fun decodeToken(token: String): String {
        if (!token.startsWith(TOK_PREFIX)) throw RuntimeException("disposablemail-com: 无效的 token")
        val raw = String(Base64.getDecoder().decode(token.substring(TOK_PREFIX.length)), StandardCharsets.UTF_8)
        val obj = Http.json.parseToJsonElement(raw) as? JsonObject
            ?: throw RuntimeException("disposablemail-com: 无效的 token")
        return jsonStr(obj, "c")
    }

    /** 合并 cookie 串与响应新 cookie。 */
    private fun mergeCookies(prev: String, resp: HttpResp): String {
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
