package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * ExpressInboxHub 渠道（expressinboxhub.com）。
 *
 * GET / 取 CSRF token 和 session cookies，POST /messages 创建邮箱并收信。
 * token 存储 JSON {csrfToken, cookies}。
 */
object ExpressInboxHub : Provider {

    private const val BASE = "https://expressinboxhub.com"
    private val CSRF_RE = Regex("<meta\\s+name=[\"']csrf-token[\"']\\s+content=[\"']([^\"']+)[\"']")

    private fun headers() = mapOf(
        "Accept" to "application/json, text/plain, */*",
        "Content-Type" to "application/json",
        "Origin" to BASE,
        "Referer" to "$BASE/",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 初始化 session，返回 [csrf, cookies]。 */
    private suspend fun initSession(): Pair<String, String> {
        val resp = ProviderUtil.httpGet(BASE, mapOf("User-Agent" to "Mozilla/5.0", "Accept" to "text/html"))
        resp.ensureSuccess()
        val csrf = CSRF_RE.find(resp.body)?.groupValues?.get(1)
            ?: throw RuntimeException("expressinboxhub: 无法提取 CSRF token")
        val cookies = resp.setCookies.joinToString("; ") { it.split(";")[0].trim() }
        if (cookies.isBlank()) throw RuntimeException("expressinboxhub: 未获取到 session cookies")
        return csrf to cookies
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val (csrf, cookies) = initSession()
        val h = headers() + mapOf("X-CSRF-TOKEN" to csrf, "Cookie" to cookies)
        val payload = buildJsonObject { put("_token", csrf) }.toString()
        val resp = ProviderUtil.httpPost("$BASE/messages", payload, "application/json", h)
        resp.ensureSuccess()
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("expressinboxhub: 响应格式异常")
        val mailbox = jsonStr(body, "mailbox").trim()
        if (mailbox.isBlank()) throw RuntimeException("expressinboxhub: 响应中缺少 mailbox")
        val token = buildJsonObject { put("csrfToken", csrf); put("cookies", cookies) }.toString()
        return EmailInfo(email = mailbox, channel = "expressinboxhub", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        if (info.token.isBlank()) throw RuntimeException("expressinboxhub: token 为空")
        val tok = Http.json.parseToJsonElement(info.token) as? JsonObject
            ?: throw RuntimeException("expressinboxhub: token 格式无效")
        val csrf = jsonStr(tok, "csrfToken")
        val cookies = jsonStr(tok, "cookies")
        val h = headers() + mapOf("X-CSRF-TOKEN" to csrf, "Cookie" to cookies)
        val payload = buildJsonObject { put("_token", csrf) }.toString()
        val resp = ProviderUtil.httpPost("$BASE/messages", payload, "application/json", h)
        resp.ensureSuccess()
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val messages = body["messages"] as? JsonArray ?: return emptyList()
        return messages.mapNotNull { it as? JsonObject }.map { item ->
            val flat = mutableMapOf<String, Any?>()
            item.forEach { (k, v) -> flat[k] = (v as? JsonPrimitive)?.contentOrNull ?: v.toString() }
            // 映射 receivedAt → date, content → html
            if (flat.containsKey("receivedAt") && !flat.containsKey("date")) flat["date"] = flat["receivedAt"]
            if (flat.containsKey("content") && !flat.containsKey("html")) flat["html"] = flat["content"]
            Normalize.fromMap(flat, info.email)
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
