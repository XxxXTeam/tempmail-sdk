package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.JsonObject

/**
 * GrabMail 渠道（grabmail.io）。
 *
 * 地址即邮箱：首封邮件到达时自动存在，无需创建。
 *
 * @property channel 对外渠道标识
 */
class Grabmail(private val channel: String) : Provider {

    private val headers = mapOf("Accept" to "application/json")

    private fun fromValue(raw: JsonObject): String {
        val name = ProviderUtil.str(raw, "from_name")
        val address = ProviderUtil.str(raw, "from")
        return if (name.isEmpty()) address else "$name <$address>"
    }

    /** 生成 GrabMail 地址（无需请求服务端）。 */
    override suspend fun generate(): EmailInfo {
        return EmailInfo(email = "${ProviderUtil.randomLocalSdk()}@$DOMAIN", channel = channel)
    }

    /** 获取 GrabMail 邮件，逐条补全正文。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("grabmail: empty email")
        val encoded = ProviderUtil.urlEncode(address).replace("%40", "@")
        val resp = ProviderUtil.httpGet("$API_BASE/mailbox?address=$encoded", headers)
        resp.ensureSuccess()
        val rows = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "messages")
            ?: return emptyList()

        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val messageId = ProviderUtil.str(item, "id").trim()
            if (messageId.isEmpty()) continue
            val detail = fetchDetail(messageId, encoded)
            if (detail != null) {
                result.add(
                    Normalize.fromMap(
                        mapOf(
                            "id" to ProviderUtil.firstNonEmpty(
                                ProviderUtil.str(detail, "id"),
                                messageId,
                            ),
                            "from" to fromValue(detail),
                            "to" to address,
                            "subject" to ProviderUtil.str(detail, "subject"),
                            "text" to ProviderUtil.str(detail, "text"),
                            "html" to ProviderUtil.str(detail, "html"),
                            "date" to ProviderUtil.str(detail, "date"),
                            "seen" to ProviderUtil.str(detail, "seen"),
                        ),
                        address,
                    ),
                )
                continue
            }
            result.add(
                Normalize.fromMap(
                    mapOf(
                        "id" to messageId,
                        "from" to fromValue(item),
                        "to" to address,
                        "subject" to ProviderUtil.str(item, "subject"),
                        "text" to ProviderUtil.str(item, "preview"),
                        "date" to ProviderUtil.str(item, "date"),
                        "seen" to ProviderUtil.str(item, "seen"),
                    ),
                    address,
                ),
            )
        }
        return result
    }

    private suspend fun fetchDetail(messageId: String, encodedAddress: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$API_BASE/message/${ProviderUtil.urlEncode(messageId)}?mailbox=$encodedAddress",
                headers,
            )
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }

    private companion object {
        const val API_BASE = "https://grabmail.io/api/v1"
        const val DOMAIN = "grabmail.io"
    }
}
