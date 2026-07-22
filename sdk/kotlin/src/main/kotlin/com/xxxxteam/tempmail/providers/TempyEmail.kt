package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Tempy Email 渠道（tempy.email）。
 *
 * POST /api/v1/mailbox 创建邮箱，GET /api/v1/mailbox/{addr}/messages 收信。
 */
object TempyEmail : Provider {

    private const val API_BASE = "https://tempy.email/api/v1"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    )
    private val POST_HEADERS = HEADERS + ("Content-Type" to "application/json")

    /** 创建临时邮箱（域名由服务端分配）。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$API_BASE/mailbox", "{}", "application/json", POST_HEADERS)
        resp.ensureSuccess()
        val obj = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("tempy-email: 无效的创建响应")
        val email = jsonStr(obj, "email").trim()
        if (email.isBlank()) throw RuntimeException("tempy-email: 无效的创建响应")
        val expiresAt = jsonStr(obj, "expires_at")
        return EmailInfo(email = email, channel = "tempy-email", expiresAt = expiresAt)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        if (addr.isBlank()) throw RuntimeException("tempy-email: 邮箱地址为空")
        val resp = ProviderUtil.httpGet(
            "$API_BASE/mailbox/${ProviderUtil.urlEncode(addr)}/messages", HEADERS)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body)
        val rows = when (data) {
            is JsonObject -> data["messages"] as? JsonArray
            is JsonArray -> data
            else -> null
        } ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { flattenMessage(it, addr) }
    }

    /** 将消息对象扁平化为标准字段并归一化。 */
    private fun flattenMessage(raw: JsonObject, recipient: String): Email {
        val to = jsonStr(raw, "to")
        val flat = mapOf(
            "id" to ProviderUtil.firstNonEmpty(
                jsonStr(raw, "id"), jsonStr(raw, "messageId"),
                jsonStr(raw, "message_id"), jsonStr(raw, "mail_id")),
            "from" to ProviderUtil.firstNonEmpty(
                jsonStr(raw, "from"), jsonStr(raw, "sender"),
                jsonStr(raw, "from_addr"), jsonStr(raw, "from_address")),
            "to" to (to.ifEmpty { recipient }),
            "subject" to ProviderUtil.firstNonEmpty(jsonStr(raw, "subject"), jsonStr(raw, "mail_title")),
            "text" to ProviderUtil.firstNonEmpty(
                jsonStr(raw, "text"), jsonStr(raw, "body_text"),
                jsonStr(raw, "text_body"), jsonStr(raw, "body")),
            "html" to ProviderUtil.firstNonEmpty(
                jsonStr(raw, "html"), jsonStr(raw, "body_html"), jsonStr(raw, "html_body")),
            "date" to ProviderUtil.firstNonEmpty(
                jsonStr(raw, "date"), jsonStr(raw, "received_at"), jsonStr(raw, "created_at")),
        )
        return Normalize.fromMap(flat, recipient)
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
