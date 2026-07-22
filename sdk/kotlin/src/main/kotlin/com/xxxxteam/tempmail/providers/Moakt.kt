package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import java.nio.charset.StandardCharsets
import java.util.Base64

/**
 * moakt.com 家族实现（HTML 抓取，Cookie 会话）。
 *
 * 流程：GET 首页取初始 Cookie → POST /inbox 创建邮箱（捕获 302 的 tm_session）
 * → GET /inbox 解析邮箱地址 → 收信时 GET /inbox 列出邮件 ID → 逐封 GET HTML 详情。
 * Token 格式：mok1:<base64({"l":"zh","c":"cookie_header"})>。
 * 参数化为 (channel, domain)，domain 可为邮件域名或 locale。
 *
 * @property channel 渠道标识
 * @property domain 域名或 locale，空串则使用默认 locale "zh"
 */
class Moakt(private val channel: String, private val domain: String) : Provider {

    private companion object {
        const val ORIGIN = "https://www.moakt.com"
        const val TOK_PREFIX = "mok1:"
        const val DEFAULT_UA =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
                "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

        val EMAIL_DIV_RE = Regex("(?is)<div\\s+id=\"email-address\"\\s*>([^<]+)</div>")
        val DOMAIN_OPTION_RE = Regex("(?is)<option\\s+value=\"([^\"]+)\">\\s*@[^<]+</option>")
        val HREF_EMAIL_RE = Regex(
            "href=\"(/[^\"/]+/email/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\"",
        )
        val TITLE_RE = Regex("(?is)<li\\s+class=\"title\"\\s*>([^<]*)</li>")
        val DATE_RE = Regex("(?is)<li\\s+class=\"date\"[^>]*>.*?<span[^>]*>([^<]+)</span>")
        val SENDER_RE = Regex("(?is)<li\\s+class=\"sender\"[^>]*>.*?<span[^>]*>(.*?)</span>\\s*</li>")
        val BODY_RE = Regex("(?is)<div\\s+class=\"email-body\"\\s*>(.*?)</div>")
        val FROM_ADDR_RE = Regex("<([a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,})>")
        val TAG_RE = Regex("<[^>]+>")
        val MAIL_DOMAIN_RE = Regex(
            "(?i)^[a-z0-9](?:[a-z0-9\\-]{0,61}[a-z0-9])?(?:\\.[a-z0-9](?:[a-z0-9\\-]{0,61}[a-z0-9])?)+$",
        )
    }

    // ── Cookie 工具 ──────────────────────────────────────────────────────────

    /** 从 Set-Cookie 列表中提取 name=value 合并到 map。 */
    private fun mergeSetCookies(dst: MutableMap<String, String>, setCookies: List<String>) {
        for (raw in setCookies) {
            val first = raw.split(";")[0].trim()
            val eq = first.indexOf('=')
            if (eq <= 0) continue
            val k = first.substring(0, eq).trim()
            val v = first.substring(eq + 1).trim()
            if (k.isNotEmpty()) dst[k] = v
        }
    }

    /** 序列化 cookie map 为 Cookie 请求头。 */
    private fun buildCookieHdr(m: Map<String, String>): String =
        m.entries.joinToString("; ") { "${it.key}=${it.value}" }

    // ── Token 编解码 ─────────────────────────────────────────────────────────

    /** 编码 token。 */
    private fun encodeToken(locale: String, cookieHdr: String): String {
        val json = "{\"l\":${jsonQuote(locale)},\"c\":${jsonQuote(cookieHdr)}}"
        return TOK_PREFIX + Base64.getEncoder().encodeToString(json.toByteArray(StandardCharsets.UTF_8))
    }

    /** 解码 token，返回 (locale, cookieHdr)。 */
    private fun decodeToken(token: String): Pair<String, String> {
        if (!token.startsWith(TOK_PREFIX)) throw RuntimeException("moakt: 无效的 session token")
        val raw = Base64.getDecoder().decode(token.substring(TOK_PREFIX.length))
        val json = String(raw, StandardCharsets.UTF_8)
        val locale = extractJsonStr(json, "l")
        val cookieHdr = extractJsonStr(json, "c")
        if (locale.isNullOrBlank() || cookieHdr.isNullOrBlank()) {
            throw RuntimeException("moakt: 无效的 session token")
        }
        return locale to cookieHdr
    }

    /** 最小化 JSON 字符串序列化。 */
    private fun jsonQuote(s: String): String =
        "\"" + s.replace("\\", "\\\\").replace("\"", "\\\"") + "\""

    /** 从简单 JSON 中提取字符串字段。 */
    private fun extractJsonStr(json: String, key: String): String? {
        val p = Regex("\"" + Regex.escape(key) + "\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"")
        val m = p.find(json) ?: return null
        return m.groupValues[1].replace("\\\"", "\"").replace("\\\\", "\\")
    }

    // ── 请求头 ────────────────────────────────────────────────────────────────

    private fun pageHeaders(referer: String, cookie: String?): Map<String, String> {
        val h = linkedMapOf(
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8",
            "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
            "Cache-Control" to "no-cache",
            "DNT" to "1",
            "Pragma" to "no-cache",
            "Upgrade-Insecure-Requests" to "1",
            "User-Agent" to DEFAULT_UA,
            "Referer" to referer,
        )
        if (!cookie.isNullOrBlank()) h["Cookie"] = cookie
        return h
    }

    // ── HTML 解析 ─────────────────────────────────────────────────────────────

    private fun stripTags(s: String): String = TAG_RE.replace(s, " ").trim()

    private fun unescapeHtml(s: String): String = s
        .replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
        .replace("&quot;", "\"").replace("&#39;", "'").replace("&nbsp;", " ")

    private fun parseInboxEmail(html: String): String {
        val m = EMAIL_DIV_RE.find(html) ?: throw RuntimeException("moakt: 未找到 email-address")
        val addr = unescapeHtml(m.groupValues[1].trim())
        if (addr.isEmpty()) throw RuntimeException("moakt: email-address 为空")
        return addr
    }

    private fun listMailIds(html: String): List<String> {
        val seen = LinkedHashSet<String>()
        for (m in HREF_EMAIL_RE.findAll(html)) {
            val path = m.groupValues[1]
            if (path.contains("/delete")) continue
            val mid = path.substringAfterLast('/')
            if (mid.length == 36) seen.add(mid)
        }
        return seen.toList()
    }

    private fun parseDetail(page: String, mid: String, recipient: String): Map<String, Any?> {
        var fromS = ""
        SENDER_RE.find(page)?.let { sm ->
            val inner = unescapeHtml(sm.groupValues[1])
            fromS = stripTags(inner)
            FROM_ADDR_RE.find(inner)?.let { fromS = it.groupValues[1].trim() }
        }
        val subj = TITLE_RE.find(page)?.let { unescapeHtml(it.groupValues[1].trim()) } ?: ""
        val dateS = DATE_RE.find(page)?.let { unescapeHtml(it.groupValues[1].trim()) } ?: ""
        val body = BODY_RE.find(page)?.groupValues?.get(1)?.trim() ?: ""
        return mapOf(
            "id" to mid,
            "to" to recipient,
            "from" to fromS,
            "subject" to subj,
            "body" to body,
            "date" to dateS,
        )
    }

    // ── 域名解析 ─────────────────────────────────────────────────────────────

    /** 解析 domain：合法邮件域名返回 ("zh", domain)，否则视为 locale 返回 (domain, "")。 */
    private fun requestParts(): Pair<String, String> {
        val s = domain.trim()
        if (s.isEmpty() || s.contains("/") || s.contains("?") || s.contains("#") || s.contains("\\")) {
            return "zh" to ""
        }
        if (MAIL_DOMAIN_RE.matches(s)) {
            return "zh" to s.removePrefix("@").lowercase()
        }
        return s to ""
    }

    private fun parseServerDomains(page: String): Set<String> {
        val set = LinkedHashSet<String>()
        for (m in DOMAIN_OPTION_RE.findAll(page)) {
            val v = m.groupValues[1].trim().removePrefix("@").lowercase()
            if (v.isNotEmpty()) set.add(v)
        }
        return set
    }

    private fun emailDomain(email: String): String {
        val at = email.lastIndexOf('@')
        return if (at >= 0) email.substring(at + 1).trim().lowercase() else ""
    }

    // ── 公开 API ─────────────────────────────────────────────────────────────

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val (locale, mailDomain) = requestParts()
        val base = "$ORIGIN/$locale"
        val inbox = "$base/inbox"

        val r1 = ProviderUtil.httpGet(base, pageHeaders(base, null))
        r1.ensureSuccess()
        val cookieMap = LinkedHashMap<String, String>()
        mergeSetCookies(cookieMap, r1.setCookies)
        var cookieHdr = buildCookieHdr(cookieMap)

        if (mailDomain.isNotEmpty()) {
            val serverDomains = parseServerDomains(r1.body)
            if (!serverDomains.contains(mailDomain)) {
                throw RuntimeException("moakt: 不支持的域名 $mailDomain")
            }
        }

        val postBody = if (mailDomain.isNotEmpty()) {
            "setemail=&username=${ProviderUtil.urlEncode(ProviderUtil.randomString(12))}" +
                "&domain=${ProviderUtil.urlEncode(mailDomain)}&preferred_domain="
        } else {
            "random=1"
        }
        val postHeaders = pageHeaders(base, cookieHdr) + ("Content-Type" to "application/x-www-form-urlencoded")
        val r2 = ProviderUtil.httpPost(
            inbox, postBody, "application/x-www-form-urlencoded", postHeaders, followRedirects = false,
        )
        mergeSetCookies(cookieMap, r2.setCookies)
        cookieHdr = buildCookieHdr(cookieMap)
        if (!cookieMap.containsKey("tm_session")) throw RuntimeException("moakt: 缺少 tm_session cookie")

        val r3 = ProviderUtil.httpGet(inbox, pageHeaders(base, cookieHdr))
        r3.ensureSuccess()
        mergeSetCookies(cookieMap, r3.setCookies)
        cookieHdr = buildCookieHdr(cookieMap)

        val email = parseInboxEmail(r3.body)
        if (mailDomain.isNotEmpty() && emailDomain(email) != mailDomain) {
            throw RuntimeException("moakt: 域名不匹配 expected=$mailDomain actual=${emailDomain(email)}")
        }
        return EmailInfo(email = email, channel = channel, token = encodeToken(locale, cookieHdr))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val (locale, cookieHdr) = decodeToken(info.token)
        val email = info.email
        val base = "$ORIGIN/$locale"
        val inbox = "$base/inbox"

        val r = ProviderUtil.httpGet(inbox, pageHeaders(base, cookieHdr))
        r.ensureSuccess()
        val ids = listMailIds(r.body)
        val out = mutableListOf<Email>()
        for (mid in ids) {
            val detailUrl = "$ORIGIN/$locale/email/$mid/html"
            val refer = "$ORIGIN/$locale/email/$mid"
            try {
                val rd = ProviderUtil.httpGet(detailUrl, pageHeaders(refer, cookieHdr))
                if (!rd.isOk) continue
                out.add(Normalize.fromMap(parseDetail(rd.body, mid, email), email))
            } catch (_: Exception) {
                // 跳过单封解析失败
            }
        }
        return out
    }
}
