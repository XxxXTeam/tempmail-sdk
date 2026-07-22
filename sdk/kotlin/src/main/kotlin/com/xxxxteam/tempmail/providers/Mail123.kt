package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Mail123 渠道（mail123.fr）。
 *
 * 无状态：generate 通过 API 申请邮箱，getEmails 以邮箱地址查询消息并按需拉取详情。
 */
object Mail123 : Provider {

    private const val API_BASE = "https://mail123.fr/api/v1"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$API_BASE/mailbox/new", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("mail123: 邮箱响应格式异常")
        val email = jsonStr(root, "address").trim()
        if (email.isEmpty() || !email.contains("@")) {
            throw RuntimeException("mail123: 邮箱响应格式异常")
        }
        var expiresAt = ""
        val days = jsonStr(root, "expires_in_days").trim().toDoubleOrNull()
        if (days != null && days > 0) {
            expiresAt = ((System.currentTimeMillis() / 1000.0 + days * 86400) * 1000).toLong().toString()
        }
        return EmailInfo(email = email, channel = "mail123", expiresAt = expiresAt)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isBlank()) throw RuntimeException("mail123: 邮箱地址为空")
        val resp = ProviderUtil.httpGet(
            "$API_BASE/mailbox/${ProviderUtil.urlEncode(address)}/messages?limit=50", HEADERS,
        )
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val rows = root["messages"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { row ->
            val messageId = jsonStr(row, "id").trim()
            val src = if (messageId.isNotEmpty()) fetchDetail(address, messageId) ?: row else row
            val flat = mapOf(
                "id" to jsonStr(src, "id"),
                "from" to jsonStr(src, "from"),
                "to" to ProviderUtil.firstNonEmpty(jsonStr(src, "to"), address),
                "subject" to jsonStr(src, "subject"),
                "body" to ProviderUtil.firstNonEmpty(jsonStr(src, "text"), jsonStr(src, "preview"), jsonStr(src, "html")),
                "date" to jsonStr(src, "date"),
            )
            Normalize.fromMap(flat, address)
        }
    }

    /** 拉取单封邮件详情。 */
    private suspend fun fetchDetail(address: String, messageId: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$API_BASE/mailbox/${ProviderUtil.urlEncode(address)}/messages/${ProviderUtil.urlEncode(messageId)}",
                HEADERS,
            )
            if (!resp.isOk) return null
            val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return null
            root["message"] as? JsonObject
        } catch (_: Exception) {
            null
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
