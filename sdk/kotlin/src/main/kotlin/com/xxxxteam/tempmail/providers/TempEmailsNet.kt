package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * tempemails.net 渠道实现 — https://tempemails.net
 *
 * Laravel CSRF + session cookie，POST /get_messages 创建/获取邮件，GET /view/{id} 正文。
 * token 为 JSON {"csrf":"...","cookies":"..."}。
 */
object TempEmailsNet : Provider {

    private const val BASE_URL = "https://tempemails.net"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

    private val PAGE_HEADERS = mapOf(
        "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language" to "en-US,en;q=0.9",
        "User-Agent" to UA,
    )

    private fun ajaxHeaders(csrf: String, cookie: String) = mapOf(
        "Accept" to "application/json",
        "X-Requested-With" to "XMLHttpRequest",
        "X-CSRF-TOKEN" to csrf,
        "Cookie" to cookie,
        "Referer" to "$BASE_URL/",
        "User-Agent" to UA,
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE_URL/", PAGE_HEADERS)
        resp.ensureSuccess()
        val csrf = ProviderUtil.extractCsrfToken(resp.body)
        if (csrf.isEmpty()) throw RuntimeException("tempemails-net: 无法提取 CSRF token")
        var cookie = ProviderUtil.extractAllCookies(resp)
        if (cookie.isEmpty()) throw RuntimeException("tempemails-net: 未获取到 session cookie")
        val resp2 = ProviderUtil.httpPost("$BASE_URL/get_messages", null, null, ajaxHeaders(csrf, cookie))
        resp2.ensureSuccess()
        val newCk = ProviderUtil.extractAllCookies(resp2)
        if (newCk.isNotEmpty()) cookie = ProviderUtil.mergeCookieStrings(cookie, newCk)
        val data = ProviderUtil.parseObject(resp2.body)
        val mailbox = ProviderUtil.str(data, "mailbox").trim()
        if (mailbox.isEmpty() || !mailbox.contains("@")) {
            throw RuntimeException("tempemails-net: 邮箱地址无效")
        }
        val token = buildJsonObject {
            put("csrf", csrf)
            put("cookies", cookie)
        }.toString()
        return EmailInfo(email = mailbox, channel = "tempemails-net", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("tempemails-net: 邮箱地址为空")
        val tokenData = ProviderUtil.parseObject(info.token.ifBlank { "{}" })
        val csrf = ProviderUtil.str(tokenData, "csrf").trim()
        val cookie = ProviderUtil.str(tokenData, "cookies").trim()
        if (csrf.isEmpty() || cookie.isEmpty()) throw RuntimeException("tempemails-net: token 无效")
        val resp = ProviderUtil.httpPost("$BASE_URL/get_messages", null, null, ajaxHeaders(csrf, cookie))
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val messages = ProviderUtil.arr(data, "messages") ?: return emptyList()
        val result = ArrayList<Email>(messages.size)
        for (item in messages) {
            val msg = item as? JsonObject ?: continue
            val msgId = ProviderUtil.str(msg, "id").trim()
            val htmlBody = fetchView(cookie, msgId)
            val fromName = ProviderUtil.str(msg, "from").trim()
            val fromEmail = ProviderUtil.str(msg, "from_email").trim()
            val fromDisplay = if (fromName.isEmpty() || fromName == fromEmail) fromEmail else "$fromName <$fromEmail>"
            val raw = mapOf(
                "id" to msgId,
                "from" to fromDisplay,
                "from_address" to fromEmail,
                "to" to address,
                "subject" to ProviderUtil.str(msg, "subject"),
                "html" to htmlBody,
                "date" to ProviderUtil.str(msg, "receivedAt"),
            )
            result.add(Normalize.fromMap(raw, address))
        }
        return result
    }

    /** 拉取单封邮件 HTML 正文，失败返回空串。 */
    private suspend fun fetchView(cookie: String, msgId: String): String {
        if (msgId.isEmpty()) return ""
        return try {
            val headers = mapOf(
                "Cookie" to cookie,
                "Referer" to "$BASE_URL/",
                "User-Agent" to UA,
                "Accept" to "text/html,*/*",
            )
            val resp = ProviderUtil.httpGet("$BASE_URL/view/${ProviderUtil.urlEncode(msgId)}", headers)
            if (resp.isOk) resp.body else ""
        } catch (_: Exception) {
            ""
        }
    }
}
