package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * M2u (MailToYou) 渠道实现 — https://m2u.io
 *
 * POST /v1/mailboxes/auto 创建邮箱，GET /v1/mailboxes/{token}/messages 收信。
 * token 为 JSON 字符串 {"token":"...","viewToken":"..."}。
 */
object M2u : Provider {

    private const val API_BASE = "https://api.m2u.io"

    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    )

    /** 打包 token 和 viewToken 为 JSON 字符串。 */
    private fun packToken(token: String, viewToken: String): String =
        buildJsonObject {
            put("token", token)
            put("viewToken", viewToken)
        }.toString()

    /** 从打包的 token 字符串中解包，返回 (token, viewToken)。 */
    private fun unpackToken(packed: String?): Pair<String, String> {
        if (packed.isNullOrBlank()) return "" to ""
        val v = packed.trim()
        if (v.startsWith("{")) {
            val obj = ProviderUtil.parseObject(v)
            if (obj != null) {
                return ProviderUtil.str(obj, "token").trim() to ProviderUtil.str(obj, "viewToken").trim()
            }
        }
        return v to ""
    }

    /** 从原始消息 JSON 抽取标准化字段。 */
    private fun flattenMessage(raw: JsonObject, recipient: String): Map<String, String> = mapOf(
        "id" to first(raw, "id", "message_id"),
        "from" to first(raw, "from_addr", "from_address", "from"),
        "to" to recipient,
        "subject" to first(raw, "subject"),
        "text" to first(raw, "text_body", "body_text", "text"),
        "html" to first(raw, "html_body", "body_html", "html"),
        "date" to first(raw, "received_at", "created_at"),
    )

    /** 依次尝试多个键，返回首个非空字符串值。 */
    private fun first(obj: JsonObject, vararg keys: String): String {
        for (k in keys) {
            val v = ProviderUtil.str(obj, k)
            if (v.isNotEmpty()) return v
        }
        return ""
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val postHeaders = HEADERS + ("Content-Type" to "application/json")
        val resp = ProviderUtil.httpPost("$API_BASE/v1/mailboxes/auto", "{}", "application/json", postHeaders)
        resp.ensureSuccess()
        val parsed = ProviderUtil.parseObject(resp.body)
            ?: throw RuntimeException("m2u: invalid mailbox response")
        val mailbox = parsed["mailbox"] as? JsonObject
            ?: throw RuntimeException("m2u: invalid mailbox response")
        val localPart = ProviderUtil.str(mailbox, "local_part").trim()
        val domain = ProviderUtil.str(mailbox, "domain").trim()
        val token = ProviderUtil.str(mailbox, "token").trim()
        val viewToken = ProviderUtil.str(mailbox, "view_token").trim()
        val email = if (localPart.isNotEmpty() && domain.isNotEmpty()) "$localPart@$domain" else ""
        if (email.isEmpty() || token.isEmpty() || viewToken.isEmpty()) {
            throw RuntimeException("m2u: invalid mailbox response")
        }
        val createdAt = ProviderUtil.str(mailbox, "created_at").trim()
        return EmailInfo(
            email = email,
            channel = "m2u",
            token = packToken(token, viewToken),
            createdAt = createdAt,
        )
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val (mailboxToken, viewToken) = unpackToken(info.token)
        val address = info.email.trim()
        if (mailboxToken.isEmpty()) throw RuntimeException("m2u: missing token")
        if (viewToken.isEmpty()) throw RuntimeException("m2u: missing view token")
        if (address.isEmpty()) throw RuntimeException("m2u: empty email")
        val resp = ProviderUtil.httpGet(
            "$API_BASE/v1/mailboxes/${ProviderUtil.urlEncode(mailboxToken)}/messages" +
                "?view=${ProviderUtil.urlEncode(viewToken)}",
            HEADERS,
        )
        resp.ensureSuccess()
        val parsed = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val rows = ProviderUtil.arr(parsed, "messages") ?: return emptyList()
        val result = ArrayList<Email>(rows.size)
        for (item in rows) {
            val row = item as? JsonObject ?: continue
            val messageId = first(row, "id", "message_id").trim()
            val detail = if (messageId.isEmpty()) null else fetchDetail(mailboxToken, viewToken, messageId)
            result.add(Normalize.fromMap(flattenMessage(detail ?: row, address), address))
        }
        return result
    }

    /** 拉取单封邮件详情，失败返回 null。 */
    private suspend fun fetchDetail(mailboxToken: String, viewToken: String, messageId: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$API_BASE/v1/mailboxes/${ProviderUtil.urlEncode(mailboxToken)}" +
                    "/messages/${ProviderUtil.urlEncode(messageId)}" +
                    "?view=${ProviderUtil.urlEncode(viewToken)}",
                HEADERS,
            )
            if (!resp.isOk) return null
            (ProviderUtil.parseObject(resp.body))?.get("message") as? JsonObject
        } catch (_: Exception) {
            null
        }
    }
}
