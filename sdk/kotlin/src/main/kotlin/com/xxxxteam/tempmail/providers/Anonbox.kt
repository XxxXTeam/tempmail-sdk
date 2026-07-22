package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider

/**
 * Anonbox 渠道（anonbox.net，CCC）。
 *
 * GET /en/ 解析 HTML 获取邮箱地址与路径令牌 "{inbox}/{secret}"，
 * GET /{inbox}/{secret}/ 返回 mbox 纯文本，逐块解析。
 */
object Anonbox : Provider {

    private const val PAGE_URL = "https://anonbox.net/en/"
    private const val BASE = "https://anonbox.net"

    private val MAIL_LINK = Regex(
        "<a href=\"https://anonbox\\.net/([a-z0-9-]+)/([A-Za-z0-9._~-]+)\">https://anonbox\\.net/[^\"]+</a>",
        RegexOption.IGNORE_CASE)
    private val DD_RE = Regex("<dd([^>]*)>([\\s\\S]*?)</dd>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val DISPLAY_NONE = Regex("display\\s*:\\s*none", RegexOption.IGNORE_CASE)
    private val P_RE = Regex("<p>([\\s\\S]*?)</p>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val SPAN_RE = Regex("<span\\b[^>]*>[\\s\\S]*?</span>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val TAG_RE = Regex("<[^>]+>")
    private val EXPIRES_RE = Regex(
        "Your mail address is valid until:</dt>\\s*<dd><p>([^<]+)</p>",
        setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val BLOCK_SPLIT = Regex("(?m)(?=^From )")
    private val BOUNDARY_RE = Regex("boundary=\"?([^\"\\s;]+)\"?", RegexOption.IGNORE_CASE)
    private val PART_CT_RE = Regex("^content-type:\\s*([^\\s;]+)", setOf(RegexOption.IGNORE_CASE, RegexOption.MULTILINE))

    private val HTML_HEADERS = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Accept" to "text/html,application/xhtml+xml",
    )
    private val MBOX_HEADERS = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Accept" to "text/plain,*/*",
    )

    /** 创建临时邮箱（解析 /en/ 页面）。 */
    override suspend fun generate(): EmailInfo {
        val r = ProviderUtil.httpGet(PAGE_URL, HTML_HEADERS)
        r.ensureSuccess()
        val (email, token, exp) = parseEnPage(r.body)
        return EmailInfo(email = email, channel = "anonbox", token = token, expiresAt = exp)
    }

    /** 获取邮件列表（mbox 格式解析）。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token.trim()
        if (token.isEmpty()) throw RuntimeException("anonbox: token 不能为空")
        val path = if (token.endsWith("/")) token else "$token/"
        val r = ProviderUtil.httpGet("$BASE/$path", MBOX_HEADERS)
        if (r.statusCode == 404) return emptyList()
        r.ensureSuccess()

        val raw = r.body.trim()
        if (raw.isEmpty()) return emptyList()
        return BLOCK_SPLIT.split(raw)
            .map { it.trim() }
            .filter { it.isNotEmpty() }
            .map { Normalize.fromMap(mboxBlockToRaw(it, info.email), info.email) }
    }

    /** 解析 /en/ 页面，返回 (邮箱, 路径令牌, 过期时间)。 */
    private fun parseEnPage(html: String): Triple<String, String, String> {
        val link = MAIL_LINK.find(html) ?: throw RuntimeException("anonbox: 未找到邮箱链接")
        val inbox = link.groupValues[1]
        val secret = link.groupValues[2]
        val token = "$inbox/$secret"

        var addressDisplay: String? = null
        for (dd in DD_RE.findAll(html)) {
            val attrs = dd.groupValues[1]
            val inner = dd.groupValues[2]
            if (DISPLAY_NONE.containsMatchIn(attrs)) continue
            val pm = P_RE.find(inner) ?: continue
            val pInner = SPAN_RE.replace(pm.groupValues[1], "")
            val display = stripTags(pInner)
            if (!display.contains("@")) continue
            val at = display.lastIndexOf('@')
            val domain = display.substring(at + 1).trim().lowercase()
            if (domain == "googlemail.com") continue
            if (domain != "$inbox.anonbox.net".lowercase()) continue
            if (display.substring(0, at).trim().isEmpty()) continue
            addressDisplay = display
            break
        }
        if (addressDisplay == null) throw RuntimeException("anonbox: 未找到邮箱地址段落")

        val at = addressDisplay.indexOf('@')
        val local = addressDisplay.substring(0, at).trim()
        if (local.isEmpty()) throw RuntimeException("anonbox: 邮箱本地部分为空")
        val emailAddr = "$local@$inbox.anonbox.net"

        val exp = EXPIRES_RE.find(html)?.groupValues?.get(1)?.trim() ?: ""
        return Triple(emailAddr, token, exp)
    }

    /** 将单个 mbox 块解析为标准字段字典。 */
    private fun mboxBlockToRaw(block: String, recipient: String): Map<String, Any?> {
        val normalized = block.replace("\r\n", "\n")
        val lines = normalized.split("\n")
        var i = 0
        if (lines.isNotEmpty() && lines[0].startsWith("From ")) i = 1

        val headers = LinkedHashMap<String, String>()
        var curKey = ""
        while (i < lines.size) {
            val line = lines[i]
            if (line.isEmpty()) { i++; break }
            if ((line[0] == ' ' || line[0] == '\t') && curKey.isNotEmpty()) {
                headers[curKey] = headers[curKey] + " " + line.trim()
            } else {
                val idx = line.indexOf(':')
                if (idx > 0) {
                    curKey = line.substring(0, idx).trim().lowercase()
                    headers[curKey] = line.substring(idx + 1).trim()
                }
            }
            i++
        }

        val bodyBuilder = StringBuilder()
        while (i < lines.size) {
            if (bodyBuilder.isNotEmpty()) bodyBuilder.append("\n")
            bodyBuilder.append(lines[i])
            i++
        }
        val body = bodyBuilder.toString()

        val ct = (headers["content-type"] ?: "").lowercase()
        var text = ""
        var htmlBody = ""
        if (ct.contains("multipart/")) {
            val boundary = BOUNDARY_RE.find(ct)?.groupValues?.get(1)
            if (boundary != null) {
                val parts = body.split("--" + Regex.escape(boundary).let { Regex(it) })
                for (rawPart in parts) {
                    val part = rawPart.trim()
                    if (part.isEmpty() || part == "--") continue
                    val sep = part.indexOf("\n\n")
                    if (sep < 0) continue
                    val ph = part.substring(0, sep)
                    val pb = part.substring(sep + 2)
                    val pct = PART_CT_RE.find(ph)?.groupValues?.get(1)?.lowercase() ?: ""
                    when (pct) {
                        "text/plain" -> text = pb.trim()
                        "text/html" -> htmlBody = pb.trim()
                    }
                }
            }
        }
        if (text.isEmpty() && htmlBody.isEmpty()) {
            if (ct.contains("text/html")) htmlBody = body.trim() else text = body.trim()
        }

        val msgId = headers["message-id"]
            ?: simpleHash(block.substring(0, minOf(512, block.length)))
        return mapOf(
            "id" to msgId,
            "from" to (headers["from"] ?: ""),
            "to" to (headers["to"] ?: recipient),
            "subject" to (headers["subject"] ?: ""),
            "text" to text,
            "html" to htmlBody,
            "date" to (headers["date"] ?: ""),
        )
    }

    /** 去除 HTML 标签并解码常见实体。 */
    private fun stripTags(html: String): String =
        TAG_RE.replace(html, "")
            .replace("&nbsp;", " ").replace("&amp;", "&")
            .replace("&lt;", "<").replace("&gt;", ">").trim()

    /** 简单 36 进制哈希，用于缺失 Message-Id 时生成稳定 id。 */
    private fun simpleHash(s: String): String {
        var h = 0L
        for (c in s) h = (h * 31 + c.code) and 0xFFFFFFFFL
        if (h == 0L) return "0"
        val digits = "0123456789abcdefghijklmnopqrstuvwxyz"
        val out = StringBuilder()
        while (h > 0) {
            out.append(digits[(h % 36).toInt()])
            h /= 36
        }
        return out.reverse().toString()
    }
}
