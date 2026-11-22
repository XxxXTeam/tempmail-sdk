package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.JsonObject

/**
 * PlainlyKit 渠道（plainlykit.com）。
 *
 * 创建邮箱返回内部令牌；内部令牌格式为 “邮箱ID|读取令牌”。
 *
 * @property channel 对外渠道标识
 */
class PlainlyKit(private val channel: String) : Provider {

    private fun splitToken(token: String): Pair<String, String> {
        val parts = token.trim().split("|", limit = 2)
        if (parts.size != 2 || parts[0].isEmpty() || parts[1].isEmpty()) {
            throw RuntimeException("plainlykit: malformed internal token")
        }
        return parts[0] to parts[1]
    }

    private fun bearerHeaders(bearer: String) = mapOf(
        "Accept" to "application/json",
        "Authorization" to "Bearer $bearer",
    )

    /** 创建 PlainlyKit 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost(
            "$API_BASE/mailboxes",
            body = "{}",
            contentType = "application/json",
            headers = mapOf("Accept" to "application/json"),
        )
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val address = ProviderUtil.str(data, "address").trim()
        val id = ProviderUtil.str(data, "id").trim()
        val token = ProviderUtil.str(data, "token").trim()
        if (address.isEmpty() || id.isEmpty() || token.isEmpty()) {
            throw RuntimeException("plainlykit: incomplete creation response")
        }
        return EmailInfo(
            email = address,
            channel = channel,
            token = "$id|$token",
            expiresAt = ProviderUtil.str(data, "expiresAt"),
            createdAt = ProviderUtil.str(data, "createdAt"),
        )
    }

    /** 获取 PlainlyKit 邮件，详情提供纯文本正文。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val (id, bearer) = splitToken(info.token)
        val base = "$API_BASE/mailboxes/${ProviderUtil.urlEncode(id)}/messages"
        val resp = ProviderUtil.httpGet(base, bearerHeaders(bearer))
        resp.ensureSuccess()
        val rows = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "messages") ?: return emptyList()

        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val messageId = ProviderUtil.str(item, "id").trim()
            if (messageId.isEmpty()) continue
            val detail = fetchDetail(base, messageId, bearer)
            if (detail != null) {
                val raw = detail["message"] as? JsonObject ?: detail
                result.add(
                    Normalize.fromMap(
                        mapOf(
                            "id" to ProviderUtil.firstNonEmpty(
                                ProviderUtil.str(raw, "id"),
                                messageId,
                            ),
                            "from" to ProviderUtil.firstNonEmpty(
                                ProviderUtil.str(raw, "sender"),
                                ProviderUtil.str(item, "sender"),
                            ),
                            "to" to info.email,
                            "subject" to ProviderUtil.firstNonEmpty(
                                ProviderUtil.str(raw, "subject"),
                                ProviderUtil.str(item, "subject"),
                            ),
                            "text" to ProviderUtil.str(raw, "text"),
                            "html" to ProviderUtil.str(raw, "html"),
                            "date" to ProviderUtil.str(raw, "receivedAt"),
                        ),
                        info.email,
                    ),
                )
                continue
            }
            // 详情失败时退回摘要
            result.add(
                Normalize.fromMap(
                    mapOf(
                        "id" to messageId,
                        "from" to ProviderUtil.str(item, "sender"),
                        "to" to info.email,
                        "subject" to ProviderUtil.str(item, "subject"),
                        "text" to ProviderUtil.str(item, "preview"),
                        "date" to ProviderUtil.str(item, "receivedAt"),
                    ),
                    info.email,
                ),
            )
        }
        return result
    }

    private suspend fun fetchDetail(base: String, messageId: String, bearer: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$base/${ProviderUtil.urlEncode(messageId)}",
                bearerHeaders(bearer),
            )
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }

    private companion object {
        const val API_BASE = "https://plainlykit.com/tools/temporary-email/api"
    }
}
