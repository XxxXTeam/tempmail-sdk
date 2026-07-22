package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider

/**
 * Eyepaste 渠道（eyepaste.com）。
 *
 * RSS XML API，无认证。本地生成随机用户名，GET /inbox/{email}.rss 收信。
 */
object Eyepaste : Provider {

    private const val DOMAIN = "eyepaste.com"
    private const val BASE = "https://www.eyepaste.com"
    private val HEADERS = mapOf(
        "Accept" to "application/rss+xml, application/xml, text/xml, */*",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    private val ITEM_RE = Regex("<item>(.*?)</item>", RegexOption.DOT_MATCHES_ALL)
    private val TITLE_RE = Regex("<title>(.*?)</title>", RegexOption.DOT_MATCHES_ALL)
    private val DESC_RE = Regex("<description>(.*?)</description>", RegexOption.DOT_MATCHES_ALL)
    private val P_TAG = Regex("<p[^>]*>(.*?)</p>", setOf(RegexOption.DOT_MATCHES_ALL, RegexOption.IGNORE_CASE))
    private val FROM_RE = Regex("From:\\s*(.*?)(?:\\s+To:|$)", RegexOption.DOT_MATCHES_ALL)
    private val TO_RE = Regex("To:\\s*(.*?)(?:\\s+Subject:|$)", RegexOption.DOT_MATCHES_ALL)
    private val SUBJECT_RE = Regex("Subject:\\s*(.*?)(?:\\s+Date:|$)", RegexOption.DOT_MATCHES_ALL)
    private val DATE_RE = Regex("Date:\\s*(.*?)$", RegexOption.DOT_MATCHES_ALL)
    private val TAG_RE = Regex("<[^>]+>")

    /** 创建临时邮箱（本地生成，无需 API 调用）。 */
    override suspend fun generate(): EmailInfo {
        val email = "${ProviderUtil.randomString(10)}@$DOMAIN"
        return EmailInfo(email = email, channel = "eyepaste")
    }

    /** 获取邮件列表（RSS XML 解析）。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        if (e.isBlank()) throw RuntimeException("eyepaste: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE/inbox/${ProviderUtil.urlEncode(e)}.rss", HEADERS)
        resp.ensureSuccess()
        val content = resp.body
        if (content.isBlank()) return emptyList()

        val out = mutableListOf<Email>()
        var idx = 0
        for (m in ITEM_RE.findAll(content)) {
            idx++
            val itemXml = m.groupValues[1]
            val title = TITLE_RE.find(itemXml)?.groupValues?.get(1)?.trim() ?: ""
            val desc = DESC_RE.find(itemXml)?.groupValues?.get(1)?.trim() ?: ""
            val parsed = parseDescription(desc)
            val subject = parsed["subject"]!!.ifEmpty { title }
            val bodyHtml = parsed["body"]!!
            val text = if (bodyHtml.isEmpty()) "" else TAG_RE.replace(bodyHtml, "").trim()
            val flat = mapOf(
                "id" to idx.toString(),
                "from" to parsed["from"],
                "to" to parsed["to"]!!.ifEmpty { e },
                "subject" to subject,
                "text" to text,
                "html" to bodyHtml,
                "date" to parsed["date"],
            )
            out.add(Normalize.fromMap(flat, e))
        }
        return out
    }

    /** 解析 RSS description 中的元数据与正文。 */
    private fun parseDescription(desc: String): Map<String, String> {
        val result = mutableMapOf("from" to "", "to" to "", "subject" to "", "date" to "", "body" to "")
        if (desc.isEmpty()) return result
        val pMatch = P_TAG.find(desc) ?: return result
        val meta = pMatch.groupValues[1].trim()
        FROM_RE.find(meta)?.let { result["from"] = it.groupValues[1].trim() }
        TO_RE.find(meta)?.let { result["to"] = it.groupValues[1].trim() }
        SUBJECT_RE.find(meta)?.let { result["subject"] = it.groupValues[1].trim() }
        DATE_RE.find(meta)?.let { result["date"] = it.groupValues[1].trim() }
        val pEnd = desc.indexOf("</p>")
        if (pEnd != -1) {
            val body = desc.substring(pEnd + 4).trim()
            if (body.isNotEmpty()) result["body"] = body
        }
        return result
    }
}
