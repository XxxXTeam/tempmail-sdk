package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * tempp-mails.com 渠道实现 — https://tempp-mails.com
 *
 * 与 tempmailten 共享模板，Laravel CSRF + session。
 * POST /messages 创建/获取邮件（首次返回 mailbox），GET /view/{id} 取正文。
 * token 使用 base64 编码，前缀 "tpm1:"。
 */
object TemppMails : Provider {

    private const val BASE_URL = "https://tempp-mails.com"
    private const val TOK_PREFIX = "tpm1:"
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

    /** 打包 CSRF + cookie 为 base64 token。 */
    private fun encodeToken(csrf: String, cookie: String): String {
        val raw = buildJsonObject {
            put("t", csrf)
            put("c", cookie)
        }.toString()
        return TOK_PREFIX + ProviderUtil.base64Encode(raw.toByteArray(Charsets.UTF_8))
    }

    /** 从 base64 token 解包，返回 (csrf, cookie)。 */
    private fun decodeToken(token: String?): Pair<String, String> {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw RuntimeException("tempp-mails: 无效的 token")
        }
        val decoded = ProviderUtil.base64Decode(token.substring(TOK_PREFIX.length))
        val obj = ProviderUtil.parseObject(String(decoded, Charsets.UTF_8))
        val csrf = ProviderUtil.str(obj, "t").trim()
        val cookie = ProviderUtil.str(obj, "c").trim()
        if (csrf.isEmpty()) throw RuntimeException("tempp-mails: token 中缺少 CSRF")
        return csrf to cookie
    }

    /** POST /messages 获取响应 JSON（含 mailbox 与 messages）。 */
    private suspend fun postMessages(csrf: String, cookie: String): JsonObject? {
        val body = "_token=${ProviderUtil.urlEncode(csrf)}&captcha="
        val resp = ProviderUtil.httpPost(
            "$BASE_URL/messages", body, "application/x-www-form-urlencoded", ajaxHeaders(csrf, cookie))
        resp.ensureSuccess()
        return ProviderUtil.parseObject(resp.body)
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val pageHeaders = mapOf(
            "User-Agent" to UA,
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        )
        val resp = ProviderUtil.httpGet("$BASE_URL/en", pageHeaders)
        resp.ensureSuccess()
        val cookie = ProviderUtil.extractAllCookies(resp)
        val csrf = ProviderUtil.extractCsrfToken(resp.body)
        if (csrf.isEmpty()) throw RuntimeException("tempp-mails: 未能从首页提取 CSRF token")
        val data = postMessages(csrf, cookie)
        val mailbox = ProviderUtil.str(data, "mailbox").trim()
        if (mailbox.isEmpty() || !mailbox.contains("@")) {
            throw RuntimeException("tempp-mails: 邮箱地址无效")
        }
        return EmailInfo(email = mailbox, channel = "tempp-mails", token = encodeToken(csrf, cookie))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("tempp-mails: 邮箱地址为空")
        val (csrf, cookie) = decodeToken(info.token)
        val data = postMessages(csrf, cookie)
        val msgs = ProviderUtil.arr(data, "messages") ?: return emptyList()
        val result = ArrayList<Email>(msgs.size)
        for (item in msgs) {
            val msg = item as? JsonObject ?: continue
            val mid = ProviderUtil.str(msg, "id").trim()
            if (mid.isEmpty()) continue
            var fromAddr = ProviderUtil.str(msg, "from_email")
            val fromName = ProviderUtil.str(msg, "from")
            if (fromName.isNotEmpty() && fromName != fromAddr) fromAddr = "$fromName <$fromAddr>"
            val raw = mapOf(
                "id" to mid,
                "from" to fromAddr,
                "to" to address,
                "subject" to ProviderUtil.str(msg, "subject"),
                "html" to fetchView(cookie, mid),
            )
            result.add(Normalize.fromMap(raw, address))
        }
        return result
    }

    /** 拉取单封邮件 HTML 正文，失败返回空串。 */
    private suspend fun fetchView(cookie: String, mid: String): String {
        if (mid.isEmpty()) return ""
        return try {
            val headers = mapOf(
                "User-Agent" to UA,
                "Accept" to "text/html,*/*",
                "Referer" to "$BASE_URL/en",
                "Cookie" to cookie,
            )
            val resp = ProviderUtil.httpGet("$BASE_URL/view/${ProviderUtil.urlEncode(mid)}", headers)
            if (resp.isOk) resp.body else ""
        } catch (_: Exception) {
            ""
        }
    }
}
