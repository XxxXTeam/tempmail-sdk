package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.JsonObject

/**
 * DummyInbox 渠道（dummyinbox.com）。
 *
 * 无需密钥；创建随机地址后，地址本身即读取凭据。
 *
 * @property channel 对外渠道标识
 */
class DummyInbox(private val channel: String) : Provider {

    private val headers = mapOf("Accept" to "application/json")

    private fun pathAddress(email: String): String =
        ProviderUtil.urlEncode(email.trim()).replace("%40", "@")

    /** 创建 DummyInbox 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$API_BASE/inboxes", headers = headers)
        resp.ensureSuccess()
        val address = ProviderUtil.str(ProviderUtil.parseObject(resp.body), "address").trim()
        if (address.isEmpty()) throw RuntimeException("dummyinbox: empty address")
        return EmailInfo(email = address, channel = channel)
    }

    /** 获取 DummyInbox 邮件，逐条补全正文。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("dummyinbox: empty email")
        val path = pathAddress(address)
        val resp = ProviderUtil.httpGet("$API_BASE/inboxes/$path/messages?limit=50", headers)
        resp.ensureSuccess()
        val rows = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "messages") ?: return emptyList()

        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val messageId = ProviderUtil.str(item, "id").trim()
            if (messageId.isEmpty()) continue
            val detail = fetchDetail(path, messageId)
            if (detail != null) {
                val toList = ProviderUtil.arr(detail, "to")
                val to = if (toList != null && toList.isNotEmpty()) {
                    toList[0].toString().trim('"')
                } else {
                    address
                }
                result.add(
                    Normalize.fromMap(
                        mapOf(
                            "id" to ProviderUtil.str(detail, "id"),
                            "from" to ProviderUtil.str(detail, "from"),
                            "to" to to.ifEmpty { address },
                            "subject" to ProviderUtil.str(detail, "subject"),
                            "text" to ProviderUtil.str(detail, "text"),
                            "html" to ProviderUtil.str(detail, "html"),
                            "date" to ProviderUtil.str(detail, "receivedAt"),
                            "attachments" to (detail["attachments"]?.toString() ?: ""),
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
                        "from" to ProviderUtil.str(item, "from"),
                        "to" to address,
                        "subject" to ProviderUtil.str(item, "subject"),
                        "text" to ProviderUtil.str(item, "snippet"),
                        "date" to ProviderUtil.str(item, "receivedAt"),
                    ),
                    address,
                ),
            )
        }
        return result
    }

    private suspend fun fetchDetail(path: String, messageId: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$API_BASE/inboxes/$path/messages/${ProviderUtil.urlEncode(messageId)}",
                headers,
            )
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)?.get("message") as? JsonObject
        } catch (_: Exception) {
            null
        }
    }

    private companion object {
        const val API_BASE = "https://dummyinbox.com/api/v1"
    }
}
