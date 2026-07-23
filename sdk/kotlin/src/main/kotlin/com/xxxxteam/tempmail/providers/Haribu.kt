package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import com.xxxxteam.tempmail.providers.ProviderUtil.str
import kotlinx.serialization.json.*
import java.util.Base64

/**
 * Haribu 渠道（haribu.net）。
 *
 * Tempail 类模式，session cookies 维持会话。首页 HTML 提取邮箱与 cookie，
 * kontrol API 触发新邮件检查后再解析首页邮件列表。
 * token 为 "haribu1:" + base64(JSON {c: cookieHeader})。
 */
object Haribu : Provider {

    private const val BASE = "https://haribu.net"
    private const val TOK_PREFIX = "haribu1:"

    private val EMAIL_INPUT_RE = Regex(
        "<input[^>]+id\\s*=\\s*[\"']eposta_adres[\"'][^>]+value\\s*=\\s*[\"']([^\"']+)[\"']", RegexOption.IGNORE_CASE)
    private val EMAIL_INPUT_RE2 = Regex(
        "<input[^>]+value\\s*=\\s*[\"']([^\"']+@[^\"']+)[\"'][^>]+id\\s*=\\s*[\"']eposta_adres[\"']", RegexOption.IGNORE_CASE)
    private val MAIL_ITEM_RE = Regex(
        "<li\\s+class\\s*=\\s*[\"']mail[\"'][^>]*>([\\s\\S]*?)</li>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val FROM_RE = Regex(
        "<span\\s+class\\s*=\\s*[\"']mail_gonderen[\"'][^>]*>([\\s\\S]*?)</span>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val SUBJECT_RE = Regex(
        "<span\\s+class\\s*=\\s*[\"']mail_konu[\"'][^>]*>([\\s\\S]*?)</span>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val DATE_RE = Regex(
        "<span\\s+class\\s*=\\s*[\"']mail_zaman[\"'][^>]*>([\\s\\S]*?)</span>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val MAIL_LINK_RE = Regex(
        "href\\s*=\\s*[\"']([^\"']*(?:mail|read|view)[^\"']*)[\"']", RegexOption.IGNORE_CASE)
    private val TAG_RE = Regex("<[^>]+>")
    private val LEADING_SLASH_RE = Regex("^/+")

    private val DEFAULT_HEADERS = mapOf(
        "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        "Cache-Control" to "no-cache",
        "DNT" to "1",
        "Pragma" to "no-cache",
        "Upgrade-Insecure-Requests" to "1",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet(BASE, DEFAULT_HEADERS)
        resp.ensureSuccess()
        val html = resp.body
        if (html.isEmpty()) throw RuntimeException("haribu: 首页响应为空")
        val email = extractEmail(html)
        val cookieHdr = extractCookieHeader(resp.setCookies)
        return EmailInfo(email = email, channel = "haribu", token = encodeToken(cookieHdr))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        if (e.isEmpty()) throw RuntimeException("haribu: 邮箱地址为空")
        val cookieHdr = decodeToken(info.token)

        // 触发 kontrol API 检查新邮件
        try {
            val h = DEFAULT_HEADERS + mapOf(
                "Cookie" to cookieHdr,
                "Referer" to BASE,
                "X-Requested-With" to "XMLHttpRequest",
            )
            ProviderUtil.httpGet("$BASE/en/api-kontrol/", h)
        } catch (_: Exception) {
        }

        val resp = ProviderUtil.httpGet(BASE, DEFAULT_HEADERS + mapOf("Cookie" to cookieHdr, "Referer" to BASE))
        resp.ensureSuccess()

        val emails = ArrayList<Email>()
        var idx = 0
        for (m in MAIL_ITEM_RE.findAll(resp.body)) {
            val content = m.groupValues[1]
            val flat = HashMap<String, Any?>()
            flat["id"] = "haribu-$idx"
            flat["to"] = e
            idx++
            FROM_RE.find(content)?.let { flat["from"] = stripTags(it.groupValues[1]).trim() }
            SUBJECT_RE.find(content)?.let { flat["subject"] = stripTags(it.groupValues[1]).trim() }
            DATE_RE.find(content)?.let { flat["date"] = stripTags(it.groupValues[1]).trim() }

            MAIL_LINK_RE.find(content)?.let { lm ->
                var detailUrl = lm.groupValues[1]
                if (!detailUrl.startsWith("http")) {
                    detailUrl = "$BASE/" + LEADING_SLASH_RE.replace(detailUrl, "")
                }
                val htmlBody = fetchDetail(detailUrl, cookieHdr)
                if (htmlBody.isNotEmpty()) flat["html"] = htmlBody
            }
            emails.add(Normalize.fromMap(flat, e))
        }
        return emails
    }

    private val BODY_OPEN_RE = Regex(
        "<div\\s+(?:id|class)\\s*=\\s*[\"'](?:mail_icerik|icerik|mail-content|message-body)[\"'][^>]*>",
        RegexOption.IGNORE_CASE,
    )

    /**
     * 使用栈式深度匹配提取正文 div 的完整内部 HTML，
     * 避免非贪婪正则在嵌套 div 时截断正文。
     */
    private fun extractBodyHtml(page: String): String {
        val m = BODY_OPEN_RE.find(page) ?: return ""
        val start = m.range.last + 1
        var pos = start
        var depth = 1
        while (pos < page.length && depth > 0) {
            val nextOpen = page.indexOf("<div", pos, ignoreCase = true)
            val nextClose = page.indexOf("</div>", pos, ignoreCase = true)
            if (nextClose == -1) break
            if (nextOpen != -1 && nextOpen < nextClose) {
                depth++
                pos = nextOpen + 4
            } else {
                depth--
                if (depth == 0) {
                    return page.substring(start, nextClose).trim()
                }
                pos = nextClose + 6
            }
        }
        return ""
    }

    /** 拉取详情页正文，失败返回空串。 */
    private suspend fun fetchDetail(url: String, cookieHdr: String): String {
        return try {
            val resp = ProviderUtil.httpGet(url, DEFAULT_HEADERS + mapOf("Cookie" to cookieHdr, "Referer" to BASE))
            if (!resp.isOk) return ""
            extractBodyHtml(resp.body)
        } catch (_: Exception) {
            ""
        }
    }

    /** 提取邮箱地址（两种属性顺序）。 */
    private fun extractEmail(html: String): String {
        EMAIL_INPUT_RE.find(html)?.groupValues?.get(1)?.trim()?.let { if (it.contains("@")) return it }
        EMAIL_INPUT_RE2.find(html)?.groupValues?.get(1)?.trim()?.let { if (it.contains("@")) return it }
        throw RuntimeException("haribu: 未找到邮箱地址（eposta_adres）")
    }

    /** 将 Set-Cookie 列表拼接为 Cookie 请求头。 */
    private fun extractCookieHeader(setCookies: List<String>): String =
        setCookies.map { it.substringBefore(";").trim() }
            .filter { it.isNotEmpty() && it.contains("=") }
            .joinToString("; ")

    /** 编码 token。 */
    private fun encodeToken(cookieHdr: String): String {
        val payload = buildJsonObject { put("c", cookieHdr) }.toString()
        return TOK_PREFIX + Base64.getEncoder().encodeToString(payload.toByteArray(Charsets.UTF_8))
    }

    /** 解码 token，取出 cookie 头。 */
    private fun decodeToken(token: String): String {
        if (!token.startsWith(TOK_PREFIX)) throw RuntimeException("haribu: 无效的会话令牌")
        return try {
            val decoded = Base64.getDecoder().decode(token.substring(TOK_PREFIX.length))
            val c = str(ProviderUtil.parseObject(String(decoded, Charsets.UTF_8)), "c")
            if (c.isEmpty()) throw RuntimeException("haribu: 会话令牌中缺少 cookie")
            c
        } catch (e: RuntimeException) {
            throw e
        } catch (e: Exception) {
            throw RuntimeException("haribu: 无效的会话令牌", e)
        }
    }

    /** 去标签并压缩空白。 */
    private fun stripTags(s: String): String = TAG_RE.replace(s, " ").trim()
}
