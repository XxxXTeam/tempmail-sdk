package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import com.xxxxteam.tempmail.providers.ProviderUtil.str
import kotlinx.serialization.json.*
import java.util.Base64

/**
 * Mohmal 渠道（www.mohmal.com）。
 *
 * 基于 HTML 解析的临时邮箱，使用 connect.sid session cookie 维持会话。
 * 创建时先以不跟随重定向的 GET 捕获 session cookie，再手动请求 inbox 页面。
 * token 为 "moh1:" + base64(JSON {c: cookieHeader})。
 */
object Mohmal : Provider {

    private const val ORIGIN = "https://www.mohmal.com"
    private const val TOK_PREFIX = "moh1:"

    private val DATA_EMAIL_RE = Regex("data-email=\"([^\"]+)\"")
    private val MESSAGE_LINK_RE = Regex("/en/message/(\\d+)")
    private val MESSAGE_BODY_OPEN_RE = Regex(
        "<div[^>]*class=\"[^\"]*(?:mail-content|message-body)[^\"]*\"[^>]*>", setOf(RegexOption.IGNORE_CASE))

    /**
     * 使用栈式深度匹配提取 mail-content/message-body div 的完整内部 HTML，
     * 避免非贪婪正则在嵌套 div 时截断正文。
     */
    private fun extractBodyHtml(page: String): String {
        val m = MESSAGE_BODY_OPEN_RE.find(page) ?: return ""
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
    private val DETAIL_FROM_RE = Regex(
        "<span[^>]*class=\"[^\"]*from[^\"]*\"[^>]*>([\\s\\S]*?)</span>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val DETAIL_SUBJECT_RE = Regex(
        "<span[^>]*class=\"[^\"]*subject[^\"]*\"[^>]*>([\\s\\S]*?)</span>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val DETAIL_DATE_RE = Regex(
        "<span[^>]*class=\"[^\"]*date[^\"]*\"[^>]*>([\\s\\S]*?)</span>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val FROM_ADDR_RE = Regex("[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}")
    private val TAG_RE = Regex("<[^>]+>")

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        // 不跟随重定向以捕获 session cookie
        val r1 = ProviderUtil.httpGet("$ORIGIN/en/create/random", pageHeaders(ORIGIN, null), followRedirects = false)
        var cookieHdr = mergeCookies("", r1.setCookies)

        // 手动跟随重定向到 inbox
        val r2 = ProviderUtil.httpGet("$ORIGIN/en/inbox", pageHeaders("$ORIGIN/en/create/random", cookieHdr))
        r2.ensureSuccess()
        cookieHdr = mergeCookies(cookieHdr, r2.setCookies)

        if (!cookieHdr.contains("connect.sid")) {
            throw RuntimeException("mohmal: 缺少 connect.sid session cookie")
        }

        val m = DATA_EMAIL_RE.find(r2.body)
            ?: throw RuntimeException("mohmal: 无法从 inbox 页面提取邮箱地址")
        val email = m.groupValues[1].replace("&amp;", "&").trim()
        if (email.isEmpty() || !email.contains("@")) {
            throw RuntimeException("mohmal: 无效的邮箱地址: $email")
        }
        return EmailInfo(email = email, channel = "mohmal", token = encodeToken(cookieHdr))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val cookieHdr = decodeToken(info.token)
        val addr = info.email.trim()

        val r = ProviderUtil.httpGet("$ORIGIN/en/inbox", pageHeaders(ORIGIN, cookieHdr))
        r.ensureSuccess()

        val seen = LinkedHashSet<String>()
        for (mm in MESSAGE_LINK_RE.findAll(r.body)) seen.add(mm.groupValues[1])
        if (seen.isEmpty()) return emptyList()

        val result = ArrayList<Email>()
        for (mid in seen) {
            val rd = ProviderUtil.httpGet("$ORIGIN/en/message/$mid", pageHeaders("$ORIGIN/en/inbox", cookieHdr))
            if (!rd.isOk) continue
            result.add(Normalize.fromMap(parseMessageDetail(rd.body, mid, addr), addr))
        }
        return result
    }

    /** 解析单封邮件详情页。 */
    private fun parseMessageDetail(page: String, mid: String, recipient: String): Map<String, Any?> {
        val raw = HashMap<String, Any?>()
        raw["id"] = mid
        raw["to"] = recipient

        DETAIL_FROM_RE.find(page)?.let {
            val fromRaw = it.groupValues[1]
            raw["from"] = FROM_ADDR_RE.find(fromRaw)?.value ?: stripTags(fromRaw)
        }
        DETAIL_SUBJECT_RE.find(page)?.let { raw["subject"] = stripTags(it.groupValues[1]) }
        DETAIL_DATE_RE.find(page)?.let { raw["date"] = stripTags(it.groupValues[1]) }
        val body = extractBodyHtml(page)
        if (body.isNotEmpty()) raw["html"] = body
        return raw
    }

    /** 构造页面请求头。 */
    private fun pageHeaders(referer: String, cookie: String?): Map<String, String> {
        val h = linkedMapOf(
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language" to "en-US,en;q=0.9",
            "Cache-Control" to "no-cache",
            "DNT" to "1",
            "Pragma" to "no-cache",
            "Upgrade-Insecure-Requests" to "1",
            "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
                "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
            "Referer" to referer,
        )
        if (!cookie.isNullOrEmpty()) h["Cookie"] = cookie
        return h
    }

    /** 合并旧 cookie 头与新 Set-Cookie，返回 Cookie 请求头。 */
    private fun mergeCookies(prev: String, setCookies: List<String>): String {
        val cookies = LinkedHashMap<String, String>()
        if (prev.isNotEmpty()) {
            for (part in prev.split(";")) {
                val p = part.trim()
                val eq = p.indexOf('=')
                if (eq > 0) cookies[p.substring(0, eq).trim()] = p.substring(eq + 1).trim()
            }
        }
        for (sc in setCookies) {
            val nv = sc.substringBefore(";").trim()
            val eq = nv.indexOf('=')
            if (eq > 0) cookies[nv.substring(0, eq).trim()] = nv.substring(eq + 1).trim()
        }
        return cookies.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }

    /** 编码 token。 */
    private fun encodeToken(cookieHdr: String): String {
        val raw = buildJsonObject { put("c", cookieHdr) }.toString()
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.toByteArray(Charsets.UTF_8))
    }

    /** 解码 token，取出 cookie 头。 */
    private fun decodeToken(tok: String): String {
        if (!tok.startsWith(TOK_PREFIX)) throw RuntimeException("mohmal: 无效的 session token")
        return try {
            val decoded = Base64.getDecoder().decode(tok.substring(TOK_PREFIX.length))
            val c = str(ProviderUtil.parseObject(String(decoded, Charsets.UTF_8)), "c")
            if (c.isEmpty()) throw RuntimeException("mohmal: token 中 cookie 为空")
            c
        } catch (e: RuntimeException) {
            throw e
        } catch (e: Exception) {
            throw RuntimeException("mohmal: 无效的 session token", e)
        }
    }

    /** 去标签并解码常见实体。 */
    private fun stripTags(s: String): String =
        TAG_RE.replace(s, "")
            .replace("&nbsp;", " ").replace("&amp;", "&")
            .replace("&lt;", "<").replace("&gt;", ">").trim()
}
