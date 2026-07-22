package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * shitty.email 临时邮箱渠道。
 *
 * POST /api/inbox 创建邮箱，GET /api/inbox 获取邮件列表，GET /api/email/{id} 获取详情。
 */
object ShittyEmail : Provider {

    private const val CHANNEL = "shitty-email"
    private const val API_BASE = "https://shitty.email/api"

    private val headers = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$API_BASE/inbox", null, null, headers)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val email = ProviderUtil.str(data, "email").trim()
        val token = ProviderUtil.str(data, "token").trim()
        if (email.isEmpty() || !email.contains("@") || token.isEmpty()) {
            throw RuntimeException("shitty-email: 创建邮箱响应无效")
        }
        return EmailInfo(email = email, channel = CHANNEL, token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val sessionToken = info.token.trim()
        val address = info.email.trim()
        if (sessionToken.isEmpty()) throw RuntimeException("shitty-email: token 为空")
        if (address.isEmpty()) throw RuntimeException("shitty-email: 邮箱地址为空")
        val h = headers + ("X-Session-Token" to sessionToken)
        val resp = ProviderUtil.httpGet("$API_BASE/inbox", h)
        resp.ensureSuccess()
        val rows = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "emails") ?: return emptyList()
        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val msgId = ProviderUtil.str(item, "id").trim()
            val detail = if (msgId.isNotEmpty()) fetchDetail(sessionToken, msgId) else null
            val raw = detail ?: item
            val map = raw.toMutableMap()
            map.putIfAbsent("to", JsonPrimitive(address))
            result.add(Normalize.fromJson(JsonObject(map), address))
        }
        return result
    }

    private suspend fun fetchDetail(token: String, messageId: String): JsonObject? {
        return try {
            val h = headers + ("X-Session-Token" to token)
            val resp = ProviderUtil.httpGet("$API_BASE/email/${ProviderUtil.urlEncode(messageId)}", h)
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }
}
