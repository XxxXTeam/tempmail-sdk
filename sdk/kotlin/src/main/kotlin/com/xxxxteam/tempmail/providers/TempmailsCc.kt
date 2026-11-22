package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.JsonObject

/**
 * tempmails.cc 渠道（tempmails.cc）。
 *
 * 地址即邮箱：在公开免费域名上生成地址，无需注册。
 * 详情正文位于 textBody / htmlBody。
 *
 * @property channel 对外渠道标识
 */
class TempmailsCc(private val channel: String) : Provider {

    private val headers = mapOf("Accept" to "application/json")

    private suspend fun pickDomain(): String {
        val resp = ProviderUtil.httpGet("$API_BASE/api/domains", headers)
        resp.ensureSuccess()
        val items = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "domainInfo")
            ?: emptyList()
        for (item in items.filterIsInstance<JsonObject>()) {
            val domain = ProviderUtil.str(item, "domain").trim()
            if (domain.isNotEmpty()) return domain
        }
        throw RuntimeException("tempmails-cc: no public domain")
    }

    /** 在公开免费域名上生成地址。 */
    override suspend fun generate(): EmailInfo {
        val domain = pickDomain()
        return EmailInfo(
            email = "${ProviderUtil.randomLocalSdk()}@$domain",
            channel = channel,
        )
    }

    /** 跳过选域，在指定固定域名上生成地址。 */
    fun generateAtDomain(domain: String): EmailInfo {
        val selected = domain.trim()
        if (selected.isEmpty()) throw RuntimeException("tempmails-cc: empty domain")
        return EmailInfo(
            email = "${ProviderUtil.randomLocalSdk()}@$selected",
            channel = channel,
        )
    }

    /** 获取 tempmails.cc 邮件，详情正文位于 textBody / htmlBody。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("tempmails-cc: empty email")
        val encoded = ProviderUtil.urlEncode(address).replace("%40", "@")
        val resp = ProviderUtil.httpGet("$API_BASE/api/inbox/$encoded", headers)
        resp.ensureSuccess()
        val rows = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "messages")
            ?: return emptyList()

        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val messageId = ProviderUtil.str(item, "id").trim()
            if (messageId.isEmpty()) continue
            val detail = fetchDetail(messageId, encoded)
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
                            "id" to ProviderUtil.firstNonEmpty(
                                ProviderUtil.str(detail, "id"),
                                messageId,
                            ),
                            "from" to ProviderUtil.str(detail, "from"),
                            "to" to to.ifEmpty { address },
                            "subject" to ProviderUtil.str(detail, "subject"),
                            "text" to ProviderUtil.str(detail, "textBody"),
                            "html" to ProviderUtil.str(detail, "htmlBody"),
                            "date" to ProviderUtil.str(detail, "date"),
                            "seen" to ProviderUtil.str(detail, "isRead"),
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
                        "from" to ProviderUtil.str(item, "from"),
                        "to" to address,
                        "subject" to ProviderUtil.str(item, "subject"),
                        "date" to ProviderUtil.str(item, "date"),
                        "seen" to ProviderUtil.str(item, "isRead"),
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
                "$API_BASE/api/message/${ProviderUtil.urlEncode(messageId)}" +
                    "?address=$encodedAddress&mark_read=false",
                headers,
            )
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }

    private companion object {
        const val API_BASE = "https://tempmails.cc"
    }
}
