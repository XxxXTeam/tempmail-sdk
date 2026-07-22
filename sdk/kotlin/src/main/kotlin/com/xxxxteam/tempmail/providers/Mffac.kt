package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.time.Instant
import java.time.format.DateTimeFormatter

/**
 * MFFAC 渠道实现 — https://www.mffac.com/api
 */
object Mffac : Provider {

    private const val CHANNEL = "mffac"
    private const val BASE = "https://www.mffac.com/api"

    private val headers = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Content-Type" to "application/json",
        "Accept" to "*/*",
        "Origin" to "https://www.mffac.com",
        "Referer" to "https://www.mffac.com/",
        "sec-ch-ua" to "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        "sec-ch-ua-mobile" to "?0",
        "sec-ch-ua-platform" to "\"Windows\"",
        "sec-fetch-dest" to "empty",
        "sec-fetch-mode" to "cors",
        "sec-fetch-site" to "same-origin",
    )

    private val getHeaders = headers.filterKeys { it != "Content-Type" }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val payload = buildJsonObject { put("expiresInHours", 24) }.toString()
        val resp = ProviderUtil.httpPost("$BASE/mailboxes", payload, "application/json", headers)
        resp.ensureSuccess()
        val root = ProviderUtil.parseObject(resp.body) ?: throw RuntimeException("mffac: 创建失败")
        if (!ProviderUtil.bool(root, "success") || root["mailbox"] !is JsonObject) {
            throw RuntimeException("mffac: 创建失败")
        }
        val mb = root["mailbox"] as JsonObject
        val addr = ProviderUtil.str(mb, "address").trim()
        val mid = ProviderUtil.str(mb, "id").trim()
        if (addr.isEmpty() || mid.isEmpty()) throw RuntimeException("mffac: 无效的邮箱响应")
        return EmailInfo(
            email = "$addr@mffac.com",
            channel = CHANNEL,
            token = mid,
            createdAt = ProviderUtil.str(mb, "createdAt"),
        )
    }

    /** 获取邮件列表（按地址本地名查询）。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val local = if (info.email.contains("@")) info.email.substringBefore("@").trim() else ""
        if (local.isEmpty()) throw RuntimeException("mffac: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE/mailboxes/$local/emails", getHeaders)
        resp.ensureSuccess()
        val root = ProviderUtil.parseObject(resp.body) ?: throw RuntimeException("mffac: 列表响应异常")
        if (!ProviderUtil.bool(root, "success")) throw RuntimeException("mffac: 列表请求失败")
        val rawList = ProviderUtil.arr(root, "emails") ?: return emptyList()
        val out = mutableListOf<Email>()
        for (item in rawList.filterIsInstance<JsonObject>()) {
            val messageId = ProviderUtil.str(item, "id").trim()
            val detail = fetchEmailDetail(messageId)
            out.add(Normalize.fromMap(flattenEmail(detail ?: item, info.email), info.email))
        }
        return out
    }

    private suspend fun fetchEmailDetail(messageId: String): JsonObject? {
        if (messageId.isEmpty()) return null
        return try {
            val r = ProviderUtil.httpGet("$BASE/emails/${ProviderUtil.urlEncode(messageId)}", getHeaders)
            if (!r.isOk) return null
            val root = ProviderUtil.parseObject(r.body) ?: return null
            if (!ProviderUtil.bool(root, "success")) return null
            root["email"] as? JsonObject
        } catch (_: Exception) {
            null
        }
    }

    /** 将 receivedAt 秒级时间戳转为 ISO 格式。 */
    private fun receivedAtToIso(raw: JsonObject): String {
        val v = raw["receivedAt"] as? JsonPrimitive ?: return ""
        return try {
            val seconds = v.content.toDouble()
            if (seconds <= 0) "" else DateTimeFormatter.ISO_INSTANT.format(Instant.ofEpochSecond(seconds.toLong()))
        } catch (_: Exception) {
            ""
        }
    }

    private fun flattenEmail(raw: JsonObject, recipient: String): Map<String, Any?> {
        val to = ProviderUtil.str(raw, "toAddress")
        return mapOf(
            "id" to ProviderUtil.str(raw, "id"),
            "from" to ProviderUtil.str(raw, "fromAddress"),
            "to" to to.ifEmpty { recipient },
            "subject" to ProviderUtil.str(raw, "subject"),
            "text" to ProviderUtil.str(raw, "textContent"),
            "html" to ProviderUtil.str(raw, "htmlContent"),
            "date" to receivedAtToIso(raw),
            "isRead" to ProviderUtil.bool(raw, "isRead"),
        )
    }
}
