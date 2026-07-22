package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider

/**
 * linshiyou.com 渠道 — NEXUS_TOKEN cookie + HTML 分片解析。
 */
object Linshiyou : Provider {

    private const val CHANNEL = "linshiyou"
    private const val ORIGIN = "https://linshiyou.com"

    private val listIdRe = Regex("id=\"tmail-email-list-([a-f0-9]+)\"", RegexOption.IGNORE_CASE)
    private val divRe = Regex("<div class=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</div>", RegexOption.IGNORE_CASE)
    private val srcdocRe = Regex("\\ssrcdoc=\"([^\"]*)\"", RegexOption.IGNORE_CASE)
    private val tagRe = Regex("<[^>]+>")

    private fun defaultHeaders(): Map<String, String> = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "accept-language" to "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "cache-control" to "no-cache",
        "dnt" to "1",
        "pragma" to "no-cache",
        "priority" to "u=1, i",
        "referer" to "$ORIGIN/",
        "sec-ch-ua" to "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        "sec-ch-ua-mobile" to "?0",
        "sec-ch-ua-platform" to "\"Windows\"",
        "sec-fetch-dest" to "empty",
        "sec-fetch-mode" to "cors",
        "sec-fetch-site" to "same-origin",
    )

    private fun stripTags(s: String?): String {
        if (s == null) return ""
        return tagRe.replace(s, " ")
            .replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">")
            .replace("&nbsp;", " ").replace(Regex("\\s+"), " ").trim()
    }

    private fun pickDiv(fragment: String, className: String): String {
        for (m in divRe.findAll(fragment)) {
            if (m.groupValues[1] == className) return stripTags(m.groupValues[2]).trim()
        }
        return ""
    }

    private fun extractSrcdoc(bodyPart: String): String {
        val m = srcdocRe.find(bodyPart) ?: return ""
        return m.groupValues[1]
            .replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"")
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$ORIGIN/api/user?user", defaultHeaders())
        resp.ensureSuccess()

        var nexus = ""
        for (sc in resp.setCookies) {
            if (sc.contains("NEXUS_TOKEN=")) {
                val start = sc.indexOf("NEXUS_TOKEN=") + "NEXUS_TOKEN=".length
                val end = sc.indexOf(';', start)
                nexus = if (end > 0) sc.substring(start, end) else sc.substring(start)
                break
            }
        }
        if (nexus.isEmpty()) throw RuntimeException("linshiyou: NEXUS_TOKEN not in Set-Cookie")

        val email = resp.body.trim()
        if (email.isEmpty() || !email.contains("@")) {
            throw RuntimeException("linshiyou: invalid email in response body")
        }
        val token = "NEXUS_TOKEN=$nexus; tmail-emails=$email"
        return EmailInfo(email = email, channel = CHANNEL, token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val h = defaultHeaders() + mapOf("Cookie" to info.token, "x-requested-with" to "XMLHttpRequest")
        val resp = ProviderUtil.httpGet("$ORIGIN/api/mail?unseen=1", h)
        resp.ensureSuccess()
        return parseSegments(resp.body, info.email)
    }

    private fun parseSegments(raw: String, recipient: String): List<Email> {
        if (raw.isBlank()) return emptyList()
        val out = mutableListOf<Email>()
        for (rawSeg in raw.split("<-----TMAILNEXTMAIL----->")) {
            val seg = rawSeg.trim()
            if (seg.isEmpty()) continue
            val parts = seg.split("<-----TMAILCHOPPER----->", limit = 2)
            val listPart = parts[0]
            val bodyPart = if (parts.size > 1) parts[1] else ""

            val mid = listIdRe.find(listPart)?.groupValues?.get(1) ?: continue
            val fromList = pickDiv(listPart, "name")
            val subjectList = pickDiv(listPart, "subject")
            val previewList = pickDiv(listPart, "body")

            val fromBody = pickDiv(bodyPart, "tmail-email-sender")
            val timeBody = pickDiv(bodyPart, "tmail-email-time")
            val titleBody = pickDiv(bodyPart, "tmail-email-title")
            val htmlBody = extractSrcdoc(bodyPart)

            val fromAddr = fromBody.ifEmpty { fromList }
            val subject = titleBody.ifEmpty { subjectList }
            val text = previewList.ifEmpty { stripTags(htmlBody) }

            out.add(
                Normalize.fromMap(
                    mapOf(
                        "id" to mid,
                        "from" to fromAddr,
                        "to" to recipient,
                        "subject" to subject,
                        "text" to text,
                        "html" to htmlBody,
                        "date" to timeBody,
                    ),
                    recipient,
                ),
            )
        }
        return out
    }
}
