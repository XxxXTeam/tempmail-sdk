package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.JsonObject

/**
 * Tsukaisute Mail 渠道（tsukaisutemail.com）。
 *
 * 无需密钥；创建随机地址后，地址即读取凭据。
 *
 * @property channel 对外渠道标识
 */
class TsukaisuteMail(private val channel: String) : Provider {

    private val headers = mapOf("Accept" to "application/json")

    private suspend fun fetchJson(url: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(url, headers)
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }

    /** 创建 Tsukaisute Mail 临时地址。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$API_BASE/address", headers = headers)
        resp.ensureSuccess()
        val address = ProviderUtil.str(ProviderUtil.parseObject(resp.body), "address").trim()
        if (address.isEmpty()) throw RuntimeException("tsukaisutemail: empty address")
        return EmailInfo(email = address, channel = channel)
    }

    /** 获取 Tsukaisute Mail 邮件，逐条补全正文。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("tsukaisutemail: empty email")
        val path = ProviderUtil.urlEncode(address).replace("%40", "@")
        val rows = ProviderUtil.arr(fetchJson("$API_BASE/inbox/$path"), "messages")
            ?: return emptyList()

        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val messageId = ProviderUtil.str(item, "id").trim()
            if (messageId.isEmpty()) continue
            val detail = fetchJson("$API_BASE/message/${ProviderUtil.urlEncode(messageId)}")
            val message = detail?.get("message") as? JsonObject
            if (message != null) {
                result.add(
                    Normalize.fromMap(
                        mapOf(
                            "id" to ProviderUtil.str(message, "id"),
                            "from" to ProviderUtil.str(message, "from_address"),
                            "to" to address,
                            "subject" to ProviderUtil.str(message, "subject"),
                            "text" to ProviderUtil.str(message, "text_body"),
                            "html" to ProviderUtil.str(message, "html_body"),
                            "date" to ProviderUtil.str(message, "received_at"),
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
                        "text" to ProviderUtil.str(item, "snippet"),
                        "date" to ProviderUtil.str(item, "received_at"),
                    ),
                    address,
                ),
            )
        }
        return result
    }

    private companion object {
        const val API_BASE = "https://tsukaisutemail.com/api/v1"
    }
}
