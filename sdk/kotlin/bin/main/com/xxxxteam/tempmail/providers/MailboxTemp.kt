package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.JsonObject

/**
 * MailboxTemp 渠道（mailboxtemp.com）。
 *
 * 无需密钥；创建免费邮箱后，地址即读取凭据。
 *
 * @property channel 对外渠道标识
 */
class MailboxTemp(private val channel: String) : Provider {

    private val headers = mapOf("Accept" to "application/json")

    private fun ok(resp: HttpResp): Boolean =
        resp.isOk && (ProviderUtil.parseObject(resp.body)?.get("ok")?.toString() != "false")

    private suspend fun fetchJson(url: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(url, headers)
            if (!ok(resp)) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }

    /** 创建 MailboxTemp 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$API_BASE/api/inbox/generate", headers = headers)
        val data = ProviderUtil.parseObject(resp.body)
        val address = ProviderUtil.str(data, "address").trim()
        if (!resp.isOk || data?.get("ok")?.toString() == "false" || address.isEmpty()) {
            throw RuntimeException("mailboxtemp: create inbox failed HTTP ${resp.statusCode}")
        }
        return EmailInfo(email = address, channel = channel, expiresAt = ProviderUtil.str(data, "expires"))
    }

    /** 获取 MailboxTemp 邮件，逐条补全正文。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("mailboxtemp: empty email")
        val path = ProviderUtil.urlEncode(address)
        val rows = ProviderUtil.arr(fetchJson("$API_BASE/api/inbox/$path/emails"), "emails")
            ?: return emptyList()

        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val messageId = ProviderUtil.str(item, "id").trim()
            if (messageId.isEmpty()) continue
            val detail = fetchJson("$API_BASE/api/email/$path/${ProviderUtil.urlEncode(messageId)}")
            val message = detail?.get("email") as? JsonObject
            if (message != null) {
                result.add(
                    Normalize.fromMap(
                        mapOf(
                            "id" to ProviderUtil.str(message, "id"),
                            "from" to ProviderUtil.str(message, "from_address"),
                            "to" to address,
                            "subject" to ProviderUtil.str(message, "subject"),
                            "text" to ProviderUtil.str(message, "body_text"),
                            "html" to ProviderUtil.str(message, "body_html"),
                            "date" to ProviderUtil.str(message, "received_at"),
                            "read" to ProviderUtil.str(message, "read"),
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
                        "from" to ProviderUtil.str(item, "from_address"),
                        "to" to address,
                        "subject" to ProviderUtil.str(item, "subject"),
                        "date" to ProviderUtil.str(item, "received_at"),
                        "read" to ProviderUtil.str(item, "read"),
                    ),
                    address,
                ),
            )
        }
        return result
    }

    private companion object {
        const val API_BASE = "https://mailboxtemp.com"
    }
}
