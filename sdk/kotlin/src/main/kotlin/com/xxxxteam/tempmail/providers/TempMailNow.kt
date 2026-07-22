package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Temp-Mail.Now 渠道实现 — https://temp-mail.now
 *
 * Cookie 会话认证：GET /en/ 获取 session cookie，
 * POST /change_email 创建邮箱，GET /fetch_emails 收信。
 * token 为合并后的 session cookie 字符串。
 */
object TempMailNow : Provider {

    private const val BASE_URL = "https://temp-mail.now"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"

    private val PAGE_HEADERS = mapOf(
        "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "User-Agent" to UA,
    )

    private fun apiHeaders(cookie: String) = mapOf(
        "Accept" to "application/json, text/javascript, */*; q=0.01",
        "X-Requested-With" to "XMLHttpRequest",
        "Referer" to "$BASE_URL/en/",
        "User-Agent" to UA,
        "Cookie" to cookie,
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE_URL/en/", PAGE_HEADERS)
        resp.ensureSuccess()
        var cookie = ProviderUtil.extractAllCookies(resp)
        if (cookie.isEmpty()) throw RuntimeException("temp-mail-now: 无法获取 session cookie")
        val resp2 = ProviderUtil.httpPost("$BASE_URL/change_email", null, null, apiHeaders(cookie))
        resp2.ensureSuccess()
        val newCookies = ProviderUtil.extractAllCookies(resp2)
        if (newCookies.isNotEmpty()) cookie = ProviderUtil.mergeCookieStrings(cookie, newCookies)
        val data = ProviderUtil.parseObject(resp2.body)
        val address = ProviderUtil.str(data, "new_email").trim()
        if (address.isEmpty() || !address.contains("@")) {
            throw RuntimeException("temp-mail-now: 创建邮箱失败")
        }
        return EmailInfo(email = address, channel = "temp-mail-now", token = cookie)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val cookie = info.token.trim()
        val address = info.email.trim()
        if (cookie.isEmpty()) throw RuntimeException("temp-mail-now: session cookie 为空")
        if (address.isEmpty()) throw RuntimeException("temp-mail-now: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE_URL/fetch_emails", apiHeaders(cookie))
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val emails = ProviderUtil.arr(data, "emails") ?: return emptyList()
        val result = ArrayList<Email>(emails.size)
        for (item in emails) {
            val msg = item as? JsonObject ?: continue
            val row = mapOf(
                "id" to ProviderUtil.str(msg, "id"),
                "from" to ProviderUtil.firstNonEmpty(
                    ProviderUtil.str(msg, "from"),
                    ProviderUtil.str(msg, "from_address"),
                    ProviderUtil.str(msg, "sender"),
                ),
                "to" to address,
                "subject" to ProviderUtil.str(msg, "subject"),
                "text" to ProviderUtil.firstNonEmpty(ProviderUtil.str(msg, "text"), ProviderUtil.str(msg, "body_text")),
                "html" to ProviderUtil.firstNonEmpty(ProviderUtil.str(msg, "html"), ProviderUtil.str(msg, "body_html")),
                "date" to ProviderUtil.firstNonEmpty(ProviderUtil.str(msg, "date"), ProviderUtil.str(msg, "received_at")),
            )
            result.add(Normalize.fromMap(row, address))
        }
        return result
    }
}
