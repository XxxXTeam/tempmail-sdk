package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider

/**
 * Mailnesia 渠道 — https://mailnesia.com
 *
 * HTML 解析模式，公开收件箱，无需认证。
 */
object Mailnesia : Provider {

    private const val CHANNEL = "mailnesia"
    private const val BASE_URL = "https://mailnesia.com"
    private const val DOMAIN = "mailnesia.com"

    private val headers = mapOf("Accept" to "text/html,*/*")

    private val rowRe = Regex(
        "<tr\\s+id=\"([^\"]+)\"[^>]*class=\"[^\"]*\\bemailheader\\b[^\"]*\"[^>]*>(.*?)</tr>",
        setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL),
    )
    private val timeRe = Regex("<time\\s+datetime=\"([^\"]+)\"", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val anchorRe = Regex("<a\\b[^>]*class=\"email\"[^>]*>(.*?)</a>", setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))
    private val tagRe = Regex("<[^>]+>", RegexOption.DOT_MATCHES_ALL)

    private fun cleanText(raw: String?): String {
        if (raw == null) return ""
        return tagRe.replace(raw, " ")
            .replace("&nbsp;", " ").replace("&amp;", "&")
            .replace("&lt;", "<").replace("&gt;", ">")
            .replace(Regex("\\s+"), " ").trim()
    }

    /** 生成随机邮箱并访问一次邮箱页面以"激活"。 */
    override suspend fun generate(): EmailInfo {
        val local = ProviderUtil.randomLocalSdk()
        try {
            ProviderUtil.httpGet("$BASE_URL/mailbox/${ProviderUtil.urlEncode(local)}", headers)
        } catch (_: Exception) {
        }
        return EmailInfo(email = "$local@$DOMAIN", channel = CHANNEL)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val local = localPart(info.email)
        if (local.isEmpty()) throw RuntimeException("mailnesia: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE_URL/mailbox/${ProviderUtil.urlEncode(local)}", headers)
        resp.ensureSuccess()
        val page = resp.body
        val result = mutableListOf<Email>()
        for (row in parseRows(page)) {
            val detail = fetchDetail(local, row)
            result.add(Normalize.fromMap(detail, info.email))
        }
        return result
    }

    private fun localPart(email: String): String {
        val trimmed = email.trim()
        val at = trimmed.indexOf('@')
        return if (at > 0) trimmed.substring(0, at) else trimmed
    }

    private fun parseRows(page: String): List<MutableMap<String, Any?>> {
        val rows = mutableListOf<MutableMap<String, Any?>>()
        for (m in rowRe.findAll(page)) {
            val messageId = m.groupValues[1].trim()
            val rowHtml = m.groupValues[2]
            val date = timeRe.find(rowHtml)?.groupValues?.get(1)?.trim() ?: ""
            val anchors = anchorRe.findAll(rowHtml).map { cleanText(it.groupValues[1]) }.toList()
            if (anchors.size < 3) continue
            rows.add(
                linkedMapOf(
                    "id" to messageId,
                    "date" to date,
                    "from" to anchors[0],
                    "to" to anchors[1],
                    "subject" to anchors[2],
                ),
            )
        }
        return rows
    }

    private suspend fun fetchDetail(local: String, row: MutableMap<String, Any?>): Map<String, Any?> {
        val messageId = row["id"] as? String ?: return row
        if (messageId.isEmpty()) return row
        try {
            val resp = ProviderUtil.httpGet(
                "$BASE_URL/mailbox/${ProviderUtil.urlEncode(local)}/${ProviderUtil.urlEncode(messageId)}",
                headers,
            )
            if (!resp.isOk) return row
            val detailPage = resp.body
            val plainRe = Regex(
                "<div\\s+id=\"text_plain_${Regex.escape(messageId)}\"[^>]*>\\s*<pre>(.*?)</pre>\\s*</div>",
                setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL),
            )
            plainRe.find(detailPage)?.let {
                row["text"] = it.groupValues[1].replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&").trim()
            }
            extractHtmlBody(detailPage, messageId)?.let { row["html"] = it }
        } catch (_: Exception) {
        }
        return row
    }

    private fun extractHtmlBody(detailPage: String, messageId: String): String? {
        val htmlDivId = "text_html_$messageId"
        val pos = detailPage.indexOf("id=\"$htmlDivId\"")
        if (pos < 0) return null
        val openEnd = detailPage.indexOf(">", pos)
        if (openEnd < 0) return null
        val start = openEnd + 1
        val plainDivId = "text_plain_$messageId"
        var end = detailPage.indexOf("<div id=\"$plainDivId\"", start)
        if (end < 0) {
            end = detailPage.indexOf("</div>", start)
            if (end >= 0) end += "</div>".length
        }
        if (end <= start) return null
        var htmlContent = detailPage.substring(start, end).trim()
        if (htmlContent.endsWith("</div>")) {
            htmlContent = htmlContent.substring(0, htmlContent.length - "</div>".length).trim()
        }
        return htmlContent
    }
}
