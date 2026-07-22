package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.*
import kotlinx.serialization.json.*

/**
 * TempMailC 渠道实现 — https://tempmailc.com
 */
object Tempmailc : Provider {

    private const val API_BASE = "https://tempmailc.com/api/v1"
    private val HEADERS = mapOf("Accept" to "application/json")

    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$API_BASE/new", HEADERS)
        resp.ensureSuccess()
        val root = ProviderUtil.parseObject(resp.body)
            ?: throw RuntimeException("tempmailc: 无效的创建响应")
        if (!ProviderUtil.bool(root, "ok")) {
            throw RuntimeException("tempmailc: 创建邮箱失败")
        }
        val email = ProviderUtil.str(root, "email").trim()
        if (email.isEmpty() || !email.contains("@")) {
            throw RuntimeException("tempmailc: 无效的邮箱地址")
        }
        return EmailInfo(email = email, channel = "tempmailc")
    }

    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val email = info.email.trim()
        if (email.isEmpty()) throw RuntimeException("tempmailc: 邮箱地址为空")
        val resp = ProviderUtil.httpGet(
            "$API_BASE/inbox?email=${ProviderUtil.urlEncode(email)}", HEADERS,
        )
        resp.ensureSuccess()
        val root = ProviderUtil.parseObject(resp.body)
            ?: throw RuntimeException("tempmailc: 收件箱响应异常")
        if (!ProviderUtil.bool(root, "ok")) {
            throw RuntimeException("tempmailc: 收件箱请求失败")
        }
        val rows = ProviderUtil.arr(root, "messages") ?: return emptyList()
        return rows.mapNotNull { item ->
            val row = item as? JsonObject ?: return@mapNotNull null
            val msgId = ProviderUtil.str(row, "id").trim()
            val detail = fetchMessage(email, msgId)
            Normalize.fromJson(detail ?: row, email)
        }
    }

    private suspend fun fetchMessage(email: String, messageId: String): JsonObject? {
        if (messageId.isEmpty()) return null
        return try {
            val resp = ProviderUtil.httpGet(
                "$API_BASE/message?email=${ProviderUtil.urlEncode(email)}&msg_id=${ProviderUtil.urlEncode(messageId)}",
                HEADERS,
            )
            if (!resp.isOk) return null
            val root = ProviderUtil.parseObject(resp.body) ?: return null
            if (root.containsKey("ok") && !ProviderUtil.bool(root, "ok")) return null
            root
        } catch (_: Exception) {
            null
        }
    }
}
