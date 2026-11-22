package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject

/**
 * Tempmailify 渠道（tempmailify.com）。
 *
 * 创建邮箱返回收件箱专属 Bearer 令牌；详情提供完整 text/html。
 *
 * @property channel 对外渠道标识
 */
class Tempmailify(private val channel: String) : Provider {

    private fun bearerHeaders(bearer: String) = mapOf(
        "Accept" to "application/json",
        "Authorization" to "Bearer $bearer",
    )

    private fun fromAddress(value: JsonElement?): String {
        return when (value) {
            is JsonObject -> ProviderUtil.str(value, "address")
            null -> ""
            else -> value.toString().trim('"')
        }
    }

    /** 创建 Tempmailify 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost(
            "$API_BASE/api/inbox",
            body = "{}",
            contentType = "application/json",
            headers = mapOf("Accept" to "application/json"),
        )
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val address = ProviderUtil.str(data, "address").trim()
        val token = ProviderUtil.str(data, "token").trim()
        if (address.isEmpty() || token.isEmpty()) {
            throw RuntimeException("tempmailify: incomplete creation response")
        }
        return EmailInfo(
            email = address,
            channel = channel,
            token = token,
            expiresAt = ProviderUtil.str(data, "expiresAt"),
            createdAt = ProviderUtil.str(data, "createdAt"),
        )
    }

    /** 获取 Tempmailify 邮件，详情提供完整 text/html。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        val bearer = info.token.trim()
        if (address.isEmpty() || bearer.isEmpty()) {
            throw RuntimeException("tempmailify: empty email or token")
        }
        val base = "$API_BASE/api/inbox/${ProviderUtil.urlEncode(address)}"
        val resp = ProviderUtil.httpGet(base, bearerHeaders(bearer))
        resp.ensureSuccess()
        val rows = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "messages")
            ?: return emptyList()

        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val messageId = ProviderUtil.str(item, "id").trim()
            if (messageId.isEmpty()) continue
            val detail = fetchDetail(base, messageId, bearer)
            if (detail != null) {
                result.add(
                    Normalize.fromMap(
                        mapOf(
                            "id" to ProviderUtil.firstNonEmpty(
                                ProviderUtil.str(detail, "id"),
                                messageId,
                            ),
                            "from" to fromAddress(detail["from"]),
                            "to" to ProviderUtil.firstNonEmpty(
                                ProviderUtil.str(detail, "to"),
                                address,
                            ),
                            "subject" to ProviderUtil.firstNonEmpty(
                                ProviderUtil.str(detail, "subject"),
                                ProviderUtil.str(item, "subject"),
                            ),
                            "text" to ProviderUtil.str(detail, "text"),
                            "html" to ProviderUtil.str(detail, "html"),
                            "date" to ProviderUtil.firstNonEmpty(
                                ProviderUtil.str(detail, "receivedAt"),
                                ProviderUtil.str(item, "receivedAt"),
                            ),
                            "seen" to ProviderUtil.str(detail, "seen"),
                        ),
                        address,
                    ),
                )
                continue
            }
            // 详情失败时退回摘要
            result.add(
                Normalize.fromMap(
                    mapOf(
                        "id" to messageId,
                        "from" to fromAddress(item["from"]),
                        "to" to address,
                        "subject" to ProviderUtil.str(item, "subject"),
                        "text" to ProviderUtil.str(item, "preview"),
                        "date" to ProviderUtil.str(item, "receivedAt"),
                        "seen" to ProviderUtil.str(item, "seen"),
                    ),
                    address,
                ),
            )
        }
        return result
    }

    private suspend fun fetchDetail(base: String, messageId: String, bearer: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$base/messages/${ProviderUtil.urlEncode(messageId)}",
                bearerHeaders(bearer),
            )
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }

    private companion object {
        const val API_BASE = "https://tempmailify.com"
    }
}
