package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Temp-Mail.org 渠道 — https://temp-mail.org
 *
 * POST /mailbox 创建（返回 JWT），GET /messages 收信（Bearer 认证），token 存 JWT。
 */
object TempMailOrg : Provider {

    private const val BASE_URL = "https://web2.temp-mail.org"
    private val HEADERS = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "Origin" to "https://temp-mail.org",
        "Referer" to "https://temp-mail.org/",
    )

    /** 创建临时邮箱，token 为 JWT。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$BASE_URL/mailbox", null, null, HEADERS)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("temp-mail-org: 创建邮箱失败")
        val token = str(data, "token").trim()
        val mailbox = str(data, "mailbox").trim()
        if (token.isEmpty() || mailbox.isEmpty()) throw RuntimeException("temp-mail-org: 创建邮箱失败")
        return EmailInfo(email = mailbox, channel = "temp-mail-org", token = token)
    }
    /** 获取邮件列表，token 为 JWT。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val jwt = info.token.trim()
        val address = info.email.trim()
        if (jwt.isEmpty()) throw RuntimeException("temp-mail-org: token 为空")
        if (address.isEmpty()) throw RuntimeException("temp-mail-org: 邮箱地址为空")
        val h = HEADERS + ("Authorization" to "Bearer $jwt")
        val resp = ProviderUtil.httpGet("$BASE_URL/messages", h)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val messages = root["messages"] as? JsonArray ?: return emptyList()
        val result = ArrayList<Email>(messages.size)
        for (item in messages) {
            val msg = item as? JsonObject ?: continue
            val msgId = str(msg, "_id").trim()
            if (msgId.isEmpty()) continue
            val detail = fetchDetail(jwt, msgId)
            val row = HashMap<String, Any?>()
            row["id"] = msgId
            if (detail != null) {
                row["from"] = str(detail, "from")
                row["subject"] = str(detail, "subject")
                row["text"] = str(detail, "bodyPreview")
                row["html"] = str(detail, "bodyHtml")
                row["date"] = str(detail, "createdAt")
            } else {
                row["from"] = str(msg, "from")
                row["subject"] = str(msg, "subject")
                row["text"] = str(msg, "bodyPreview")
                row["date"] = str(msg, "receivedAt")
            }
            row["to"] = address
            result.add(Normalize.fromMap(row, address))
        }
        return result
    }

    /** 拉取单封邮件详情，失败返回 null。 */
    private suspend fun fetchDetail(jwt: String, msgId: String): JsonObject? {
        return try {
            val h = HEADERS + ("Authorization" to "Bearer $jwt")
            val resp = ProviderUtil.httpGet("$BASE_URL/messages/" + ProviderUtil.urlEncode(msgId), h)
            if (!resp.isOk) null else Http.json.parseToJsonElement(resp.body) as? JsonObject
        } catch (_: Exception) {
            null
        }
    }

    private fun str(obj: JsonObject, key: String): String =
        (obj[key] as? JsonPrimitive)?.content ?: ""
}
