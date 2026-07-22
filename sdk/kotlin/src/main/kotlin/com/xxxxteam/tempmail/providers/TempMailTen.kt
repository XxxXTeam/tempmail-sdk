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
 * tempmailten.com 渠道（tempmailten.com）。
 *
 * Laravel CSRF + session，POST /messages 取邮箱和邮件列表，GET /view/{id} 取正文。
 * token 使用 base64 编码，前缀 "tmt1:"。
 */
object TempMailTen : Provider {

    private const val BASE_URL = "https://tempmailten.com"
    private const val TOK_PREFIX = "tmt1:"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

    private fun ajaxHeaders(csrf: String, cookie: String) = mapOf(
        "User-Agent" to UA,
        "Accept" to "application/json, text/javascript, */*; q=0.01",
        "Accept-Language" to "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "X-Requested-With" to "XMLHttpRequest",
        "X-CSRF-TOKEN" to csrf,
        "Content-Type" to "application/x-www-form-urlencoded",
        "Referer" to "$BASE_URL/en",
        "Cookie" to cookie,
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE_URL/en", mapOf(
            "User-Agent" to UA,
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"))
        resp.ensureSuccess()
        val cookie = extractAllCookies(resp)
        val csrf = extractCsrfToken(resp.body)
        if (csrf.isBlank()) throw RuntimeException("tempmailten: 未能从首页提取 CSRF token")
        val data = postMessages(csrf, cookie)
        val mailbox = jsonStr(data, "mailbox").trim()
        if (mailbox.isBlank() || !mailbox.contains("@")) throw RuntimeException("tempmailten: 邮箱地址无效")
        return EmailInfo(email = mailbox, channel = "tempmailten", token = encodeToken(csrf, cookie))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isBlank()) throw RuntimeException("tempmailten: 邮箱地址为空")
        val (csrf, cookie) = decodeToken(info.token)
        val data = postMessages(csrf, cookie)
        val msgs = data["messages"] as? JsonArray ?: return emptyList()
        return msgs.mapNotNull { it as? JsonObject }.mapNotNull { item ->
            val mid = jsonStr(item, "id").trim()
            if (mid.isEmpty()) return@mapNotNull null
            var fromAddr = jsonStr(item, "from_email")
            val fromName = jsonStr(item, "from")
            if (fromName.isNotEmpty() && fromName != fromAddr) fromAddr = "$fromName <$fromAddr>"
            val flat = mapOf(
                "id" to mid,
                "from" to fromAddr,
                "to" to address,
                "subject" to jsonStr(item, "subject"),
                "html" to fetchView(mid, cookie),
            )
            Normalize.fromMap(flat, address)
        }
    }

    /** POST /messages 获取邮箱或邮件列表。 */
    private suspend fun postMessages(csrf: String, cookie: String): JsonObject {
        val body = "_token=${ProviderUtil.urlEncode(csrf)}&captcha="
        val resp = ProviderUtil.httpPost("$BASE_URL/messages", body,
            "application/x-www-form-urlencoded", ajaxHeaders(csrf, cookie))
        resp.ensureSuccess()
        return Http.json.parseToJsonElement(resp.body) as? JsonObject ?: JsonObject(emptyMap())
    }

    /** 拉取单封邮件 HTML 正文，失败返回空串。 */
    private suspend fun fetchView(mid: String, cookie: String): String {
        return try {
            val h = mapOf(
                "User-Agent" to UA, "Accept" to "text/html,*/*",
                "Referer" to "$BASE_URL/en", "Cookie" to cookie)
            val resp = ProviderUtil.httpGet("$BASE_URL/view/${ProviderUtil.urlEncode(mid)}", h)
            if (resp.isOk) resp.body else ""
        } catch (_: Exception) {
            ""
        }
    }

    /** 编码 token（base64，前缀 tmt1:）。 */
    private fun encodeToken(csrf: String, cookie: String): String {
        val raw = buildJsonObject { put("t", csrf); put("c", cookie) }.toString()
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.toByteArray(StandardCharsets.UTF_8))
    }

    /** 解码 token，返回 [csrf, cookie]。 */
    private fun decodeToken(token: String): Pair<String, String> {
        if (!token.startsWith(TOK_PREFIX)) throw RuntimeException("tempmailten: 无效的 token")
        val decoded = Base64.getDecoder().decode(token.substring(TOK_PREFIX.length))
        val obj = Http.json.parseToJsonElement(String(decoded, StandardCharsets.UTF_8)) as? JsonObject
            ?: throw RuntimeException("tempmailten: 无效的 token")
        val csrf = jsonStr(obj, "t").trim()
        val cookie = jsonStr(obj, "c").trim()
        if (csrf.isEmpty()) throw RuntimeException("tempmailten: token 中缺少 CSRF")
        return csrf to cookie
    }

    /** 提取响应中所有 Set-Cookie 的 name=value 串。 */
    private fun extractAllCookies(resp: HttpResp): String =
        resp.setCookies.map { it.split(";")[0].trim() }.filter { it.contains("=") }.joinToString("; ")

    /** 从 HTML 提取 Laravel CSRF token。 */
    private fun extractCsrfToken(html: String): String {
        val patterns = listOf(
            Regex("<meta\\s+name=[\"']csrf-token[\"']\\s+content=[\"']([^\"']+)[\"']"),
            Regex("name=[\"']_token[\"']\\s+value=[\"']([^\"']+)[\"']"),
        )
        for (re in patterns) re.find(html)?.let { return it.groupValues[1] }
        return ""
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
