package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * tempemail.co 渠道实现 — https://tempemail.co
 *
 * GET /mail/random 创建，GET /get-mails 获取邮件列表（HTML regex 提取 ID），
 * GET /mail/info?id={id} 获取邮件详情。
 * token 为 JSON {"address":"...","cookies":"..."}。
 */
object TempEmailCo : Provider {

    private const val BASE_URL = "https://tempemail.co"
    private val MAIL_ID_RE = Regex("data-id=\"(\\d+)\"")
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

    private fun jsonHeaders(cookie: String): Map<String, String> {
        val h = linkedMapOf(
            "User-Agent" to UA,
            "Accept" to "application/json, text/javascript, */*; q=0.01",
            "Referer" to "$BASE_URL/",
        )
        if (cookie.isNotEmpty()) h["Cookie"] = cookie
        return h
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE_URL/mail/random", jsonHeaders(""))
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        if (data == null || !ProviderUtil.bool(data, "result")) {
            throw RuntimeException("tempemail-co: 创建邮箱失败")
        }
        val address = ProviderUtil.firstNonEmpty(
            ProviderUtil.str(data, "address"), ProviderUtil.str(data, "id")).trim()
        if (address.isEmpty() || !address.contains("@")) {
            throw RuntimeException("tempemail-co: 邮箱地址无效")
        }
        val cookie = ProviderUtil.extractAllCookies(resp)
        val token = buildJsonObject {
            put("address", address)
            put("cookies", cookie)
        }.toString()
        return EmailInfo(email = address, channel = "tempemail-co", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("tempemail-co: 邮箱地址为空")
        val tokenData = ProviderUtil.parseObject(info.token.ifBlank { "{}" })
        val storedAddr = ProviderUtil.str(tokenData, "address").trim()
        val cookie = ProviderUtil.str(tokenData, "cookies").trim()
        val queryAddr = storedAddr.ifEmpty { address }
        val url = "$BASE_URL/get-mails?mail_id=${ProviderUtil.urlEncode(queryAddr)}&unseen=0&is_new=1"
        val resp = ProviderUtil.httpGet(url, jsonHeaders(cookie))
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val count = (data["count"] as? JsonPrimitive)?.content?.toIntOrNull() ?: 0
        if (count <= 0) return emptyList()
        val mailsHtml = ProviderUtil.str(data, "mails")
        if (mailsHtml.isEmpty()) return emptyList()
        val uniqueIds = LinkedHashSet<String>()
        MAIL_ID_RE.findAll(mailsHtml).forEach { uniqueIds.add(it.groupValues[1]) }
        if (uniqueIds.isEmpty()) return emptyList()
        val result = ArrayList<Email>(uniqueIds.size)
        for (mailId in uniqueIds) {
            val raw = fetchMailInfo(cookie, mailId, address)
            if (raw != null) result.add(Normalize.fromMap(raw, address))
        }
        return result
    }

    /** 拉取单封邮件详情，失败返回 null。 */
    private suspend fun fetchMailInfo(cookie: String, mailId: String, recipient: String): Map<String, String>? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$BASE_URL/mail/info?id=${ProviderUtil.urlEncode(mailId)}", jsonHeaders(cookie))
            if (!resp.isOk) return null
            val data = ProviderUtil.parseObject(resp.body) ?: return null
            if (!ProviderUtil.bool(data, "result")) return null
            val mail = data["mail"] as? JsonObject ?: return null
            val fromName = ProviderUtil.str(mail, "fromName").trim()
            val fromAddr = ProviderUtil.str(mail, "fromAddress").trim()
            val fromDisplay = if (fromName.isEmpty()) fromAddr else "$fromName <$fromAddr>"
            mapOf(
                "id" to mailId,
                "from" to fromDisplay,
                "from_address" to fromAddr,
                "to" to recipient,
                "subject" to ProviderUtil.str(mail, "subject"),
                "html" to ProviderUtil.str(mail, "textHtml"),
                "date" to ProviderUtil.str(mail, "displayDate"),
            )
        } catch (_: Exception) {
            null
        }
    }
}
