package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import com.xxxxteam.tempmail.providers.ProviderUtil.str
import kotlinx.serialization.json.*

/**
 * Lroid 渠道（lroid.com）。
 *
 * HTML 解析模式，使用 session cookies 维持邮箱身份。token 存储 JSON {cookie}。
 * 优先尝试 kontrol API，失败回退到首页 HTML 解析。
 */
object Lroid : Provider {

    private const val BASE_URL = "https://lroid.com"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

    private val EMAIL_RE = Regex(
        "<input[^>]+id=[\"']eposta_adres[\"'][^>]+value=[\"']([^\"']+@[^\"']+)[\"']", RegexOption.IGNORE_CASE)
    private val EMAIL_RE_ALT = Regex(
        "<input[^>]+value=[\"']([^\"']+@[^\"']+)[\"'][^>]+id=[\"']eposta_adres[\"']", RegexOption.IGNORE_CASE)
    private val TAG_RE = Regex("<[^>]+>")
    private val MAIL_ITEM_RE = Regex(
        "<li[^>]*class=[\"'][^\"']*\\bmail\\b[^\"']*[\"'][^>]*>(.*?)</li>",
        setOf(RegexOption.DOT_MATCHES_ALL, RegexOption.IGNORE_CASE))
    private val SUBJ_RE = Regex(
        "class=[\"'][^\"']*\\bsubject\\b[^\"']*[\"'][^>]*>(.*?)</",
        setOf(RegexOption.DOT_MATCHES_ALL, RegexOption.IGNORE_CASE))
    private val FROM_RE = Regex(
        "class=[\"'][^\"']*\\b(?:from|sender)\\b[^\"']*[\"'][^>]*>(.*?)</",
        setOf(RegexOption.DOT_MATCHES_ALL, RegexOption.IGNORE_CASE))
    private val DATE_RE = Regex(
        "class=[\"'][^\"']*\\b(?:date|time)\\b[^\"']*[\"'][^>]*>(.*?)</",
        setOf(RegexOption.DOT_MATCHES_ALL, RegexOption.IGNORE_CASE))

    private val PAGE_HEADERS = mapOf(
        "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language" to "en-US,en;q=0.9",
        "Referer" to "$BASE_URL/",
        "User-Agent" to UA,
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet(BASE_URL, PAGE_HEADERS)
        resp.ensureSuccess()
        val email = extractEmail(resp.body)
        val cookieHeader = extractCookieHeader(resp.setCookies)
        val token = buildJsonObject { put("cookie", cookieHeader) }.toString()
        return EmailInfo(email = email, channel = "lroid", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        if (info.token.isBlank()) throw RuntimeException("lroid: token 不能为空")
        val addr = info.email.trim()
        if (addr.isEmpty()) throw RuntimeException("lroid: 邮箱地址不能为空")
        val cookie = str(ProviderUtil.parseObject(info.token), "cookie")
        if (cookie.isEmpty()) throw RuntimeException("lroid: token 中缺少 cookie")

        // 优先尝试 kontrol API
        try {
            val h = mapOf(
                "Accept" to "application/json, text/html, */*;q=0.8",
                "Referer" to "$BASE_URL/",
                "Cookie" to cookie,
                "User-Agent" to UA,
            )
            val resp = ProviderUtil.httpGet("$BASE_URL/en/api-kontrol/", h)
            if (resp.isOk) {
                val data = ProviderUtil.parse(resp.body)
                if (data is JsonArray) return parseJsonEmails(data, addr)
                if (data is JsonObject) {
                    for (key in arrayOf("mails", "emails", "messages", "data", "inbox")) {
                        (data[key] as? JsonArray)?.let { return parseJsonEmails(it, addr) }
                    }
                }
            }
        } catch (_: Exception) {
        }

        return parseHtmlEmails(cookie, addr)
    }

    /** 解析 JSON 数组形式的邮件列表。 */
    private fun parseJsonEmails(items: JsonArray, recipient: String): List<Email> =
        items.mapNotNull { it as? JsonObject }.map { raw ->
            val flat = mapOf(
                "id" to str(raw, "id"),
                "from" to ProviderUtil.firstNonEmpty(str(raw, "from"), str(raw, "sender")),
                "to" to str(raw, "to"),
                "subject" to str(raw, "subject"),
                "html" to ProviderUtil.firstNonEmpty(str(raw, "html"), str(raw, "body"), str(raw, "content")),
                "date" to str(raw, "date"),
            )
            Normalize.fromMap(flat, recipient)
        }

    /** 回退：解析首页 HTML 中的邮件条目。 */
    private suspend fun parseHtmlEmails(cookie: String, recipient: String): List<Email> {
        val h = PAGE_HEADERS + ("Cookie" to cookie)
        val resp = ProviderUtil.httpGet(BASE_URL, h)
        resp.ensureSuccess()

        val emails = ArrayList<Email>()
        var idx = 0
        for (m in MAIL_ITEM_RE.findAll(resp.body)) {
            idx++
            val itemHtml = m.groupValues[1]
            val flat = mapOf(
                "id" to idx.toString(),
                "to" to recipient,
                "subject" to (SUBJ_RE.find(itemHtml)?.let { TAG_RE.replace(it.groupValues[1], "").trim() } ?: ""),
                "from" to (FROM_RE.find(itemHtml)?.let { TAG_RE.replace(it.groupValues[1], "").trim() } ?: ""),
                "date" to (DATE_RE.find(itemHtml)?.let { TAG_RE.replace(it.groupValues[1], "").trim() } ?: ""),
            )
            emails.add(Normalize.fromMap(flat, recipient))
        }
        return emails
    }

    /** 从 HTML 提取邮箱地址（两种输入属性顺序）。 */
    private fun extractEmail(html: String): String {
        EMAIL_RE.find(html)?.groupValues?.get(1)?.trim()?.let { if (it.contains("@")) return it }
        EMAIL_RE_ALT.find(html)?.groupValues?.get(1)?.trim()?.let { if (it.contains("@")) return it }
        throw RuntimeException("lroid: 无法从 HTML 响应中解析邮箱地址")
    }

    /** 将 Set-Cookie 列表拼接为 Cookie 请求头。 */
    private fun extractCookieHeader(setCookies: List<String>): String =
        setCookies.map { it.substringBefore(";").trim() }
            .filter { it.isNotEmpty() && it.contains("=") }
            .joinToString("; ")
}
