package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.util.Base64

/**
 * 10minutemail.net 渠道（10minutemail.net）。
 *
 * PHPSESSID session + HTML 表格解析，含 Cloudflare 邮箱保护解码。
 * token 为 "tmn1:" + base64(JSON{c: cookieHeader})。
 */
object TenminutemailNet : Provider {

    private const val CHANNEL = "10minutemail-net"
    private const val BASE_URL = "https://10minutemail.net"
    private const val TOK_PREFIX = "tmn1:"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

    private val EMAIL_RE = Regex("value=\"([^\"]+@[^\"]+)\"")
    private val ROW_RE = Regex("(?is)<tr[^>]*>(.*?)</tr>")
    private val MID_RE = Regex("(?i)readmail\\.html\\?mid=([^'\"&]+)")
    private val CELL_RE = Regex("(?is)<td[^>]*>(.*?)</td>")
    private val TITLE_RE = Regex("(?i)title=\"([^\"]+)\"")
    private val CF_RE = Regex("(?i)data-cfemail=\"([0-9a-fA-F]+)\"")
    private val TAG_RE = Regex("(?s)<[^>]+>")

    private fun browserHeaders() = mapOf(
        "User-Agent" to UA,
        "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "Accept-Language" to "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    )

    private fun ajaxHeaders(cookie: String): Map<String, String> {
        val h = linkedMapOf(
            "User-Agent" to UA,
            "Accept" to "*/*",
            "Accept-Language" to "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
            "X-Requested-With" to "XMLHttpRequest",
            "Referer" to "$BASE_URL/",
        )
        if (cookie.isNotEmpty()) h["Cookie"] = cookie
        return h
    }

    /** 编码 cookie 串为 token。 */
    private fun encodeToken(cookieHdr: String): String {
        val raw = buildJsonObject { put("c", cookieHdr) }.toString()
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.toByteArray(Charsets.UTF_8))
    }

    private fun decodeToken(token: String): String {
        if (!token.startsWith(TOK_PREFIX)) return ""
        return try {
            val decoded = Base64.getDecoder().decode(token.substring(TOK_PREFIX.length))
            ProviderUtil.str(ProviderUtil.parseObject(String(decoded, Charsets.UTF_8)), "c")
        } catch (_: Exception) {
            ""
        }
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE_URL/", browserHeaders())
        resp.ensureSuccess()
        val cookieHdr = ProviderUtil.extractAllCookies(resp.setCookies)
        val email = EMAIL_RE.find(resp.body)?.groupValues?.get(1)?.trim()
            ?: throw RuntimeException("10minutemail-net: 未能从首页提取邮箱地址")
        if (email.isEmpty() || !email.contains("@")) {
            throw RuntimeException("10minutemail-net: 获取到的邮箱地址无效: $email")
        }
        return EmailInfo(email = email, channel = CHANNEL, token = encodeToken(cookieHdr))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        if (addr.isEmpty()) throw IllegalArgumentException("10minutemail-net: 邮箱地址为空")
        val cookieHdr = decodeToken(info.token.trim())
        val listUrl = "$BASE_URL/mailbox.ajax.php?_=${System.currentTimeMillis()}"
        val resp = ProviderUtil.httpGet(listUrl, ajaxHeaders(cookieHdr))
        resp.ensureSuccess()
        val out = ArrayList<Email>()
        for (rowMatch in ROW_RE.findAll(resp.body)) {
            val rowInner = rowMatch.groupValues[1]
            if (rowInner.contains("<th", ignoreCase = true)) continue
            val mailId = MID_RE.find(rowInner)?.groupValues?.get(1)?.trim() ?: continue
            if (mailId.isEmpty()) continue
            val cells = CELL_RE.findAll(rowInner).map { it.groupValues[1] }.toList()
            val fromAddr = extractText(cells.getOrElse(0) { "" })
            val subject = extractText(cells.getOrElse(1) { "" })
            val dateCell = cells.getOrElse(2) { "" }
            // 日期优先取 title 属性
            val date = TITLE_RE.find(dateCell)?.groupValues?.get(1)?.trim() ?: extractText(dateCell)
            val row = mapOf(
                "id" to mailId,
                "from" to fromAddr,
                "to" to addr,
                "subject" to subject,
                "html" to fetchBody(cookieHdr, mailId),
                "date" to date,
            )
            out.add(Normalize.fromMap(row, addr))
        }
        return out
    }

    /** 拉取单封邮件正文 HTML，失败返回空串。 */
    private suspend fun fetchBody(cookieHdr: String, mailId: String): String {
        if (mailId.isEmpty()) return ""
        val h = browserHeaders().toMutableMap().apply {
            this["Referer"] = "$BASE_URL/"
            if (cookieHdr.isNotEmpty()) this["Cookie"] = cookieHdr
        }
        return try {
            val resp = ProviderUtil.httpGet("$BASE_URL/readmail.html?mid=" + ProviderUtil.urlEncode(mailId), h)
            if (!resp.isOk) "" else extractBody(resp.body)
        } catch (_: Exception) {
            ""
        }
    }

    /** 从详情页 HTML 抠出 class="mailinhtml" 容器内的正文。 */
    private fun extractBody(page: String): String {
        val startMark = "class=\"mailinhtml\""
        val si = page.indexOf(startMark)
        if (si < 0) return ""
        val gt = page.indexOf(">", si)
        if (gt < 0) return ""
        val rest = page.substring(gt + 1)
        val ei = rest.indexOf("email-decode.min.js")
        if (ei < 0) {
            val di = rest.indexOf("</div>")
            return if (di >= 0) rest.substring(0, di).trim() else rest.trim()
        }
        var segment = rest.substring(0, ei)
        val sIdx = segment.lastIndexOf("<script")
        if (sIdx >= 0) segment = segment.substring(0, sIdx)
        segment = segment.trim()
        // 去掉尾部两个 </div>
        repeat(2) {
            segment = segment.trim()
            if (segment.endsWith("</div>")) segment = segment.substring(0, segment.length - 6)
        }
        return segment.trim()
    }

    /** 提取单元格文本，优先解码 Cloudflare 邮箱保护混淆。 */
    private fun extractText(cell: String): String {
        CF_RE.find(cell)?.let { m ->
            val decoded = cfDecode(m.groupValues[1])
            if (decoded.isNotEmpty()) return decoded
        }
        return TAG_RE.replace(cell, "")
            .replace("&nbsp;", " ").replace("&#160;", " ")
            .replace("&amp;", "&").replace("&lt;", "<")
            .replace("&gt;", ">").replace("&quot;", "\"").trim()
    }

    /** Cloudflare data-cfemail 解码：首字节为异或 key。 */
    private fun cfDecode(encoded: String): String {
        val data = try {
            hexToBytes(encoded)
        } catch (_: Exception) {
            return ""
        }
        if (data.size < 2) return ""
        val key = data[0].toInt() and 0xFF
        val decoded = ByteArray(data.size - 1)
        for (i in 1 until data.size) decoded[i - 1] = ((data[i].toInt() and 0xFF) xor key).toByte()
        val result = String(decoded, Charsets.UTF_8)
        return if (result.contains("@")) result else ""
    }

    private fun hexToBytes(hex: String): ByteArray {
        val len = hex.length / 2
        return ByteArray(len) { i -> hex.substring(i * 2, i * 2 + 2).toInt(16).toByte() }
    }
}
