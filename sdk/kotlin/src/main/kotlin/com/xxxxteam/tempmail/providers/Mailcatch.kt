package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider

/**
 * MailCatch 渠道（mailcatch.com）。
 *
 * 公开收件箱，无需认证。本地生成随机用户名 + @mailcatch.com，收件箱名存入 EmailInfo.token。
 * GET /api/list/{inbox}（HTML）+ GET /api/data/{inbox}/{id} 取正文。
 */
object Mailcatch : Provider {

    private const val BASE_URL = "https://mailcatch.com"
    private const val DOMAIN = "mailcatch.com"
    private val HEADERS = mapOf(
        "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )
    private val EMAIL_ITEM_RE = Regex(
        "class=\"email-item\"\\s+data-email-id=\"([^\"]*)\"\\s+data-subject=\"([^\"]*)\"\\s+" +
            "data-timestamp=\"([^\"]*)\"\\s+data-sender=\"([^\"]*)\"",
        setOf(RegexOption.IGNORE_CASE, RegexOption.DOT_MATCHES_ALL))

    /** 创建临时邮箱（公开收件箱，无需 API 调用）。 */
    override suspend fun generate(): EmailInfo {
        val local = ProviderUtil.randomLocalSdk()
        return EmailInfo(email = "$local@$DOMAIN", channel = "mailcatch", token = local)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        if (addr.isBlank()) throw RuntimeException("mailcatch: 邮箱地址不能为空")
        var inbox = info.token.trim()
        if (inbox.isEmpty()) inbox = if (addr.contains("@")) addr.substringBefore("@") else addr

        val resp = ProviderUtil.httpGet("$BASE_URL/api/list/${ProviderUtil.urlEncode(inbox)}", HEADERS)
        resp.ensureSuccess()

        val out = mutableListOf<Email>()
        for (m in EMAIL_ITEM_RE.findAll(resp.body)) {
            val emailId = m.groupValues[1].trim()
            if (emailId.isEmpty()) continue
            val subject = m.groupValues[2].trim()
            val timestamp = m.groupValues[3].trim()
            val sender = m.groupValues[4].trim()
            val bodyHtml = fetchBody(inbox, emailId)
            val flat = mapOf(
                "id" to emailId,
                "from" to sender,
                "to" to addr,
                "subject" to subject,
                "html" to bodyHtml,
                "date" to timestamp,
            )
            out.add(Normalize.fromMap(flat, addr))
        }
        return out
    }

    /** 拉取单封邮件正文，失败返回空串。 */
    private suspend fun fetchBody(inbox: String, emailId: String): String {
        return try {
            val resp = ProviderUtil.httpGet(
                "$BASE_URL/api/data/${ProviderUtil.urlEncode(inbox)}/${ProviderUtil.urlEncode(emailId)}", HEADERS)
            if (resp.isOk) resp.body.trim() else ""
        } catch (_: Exception) {
            ""
        }
    }
}
