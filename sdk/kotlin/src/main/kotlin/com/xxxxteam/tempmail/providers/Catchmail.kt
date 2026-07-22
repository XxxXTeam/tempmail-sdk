package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Catchmail 渠道（catchmail.io）。
 *
 * 域名别名：catchmail.io / mailistry.com / zeppost.com
 *
 * @property channel 对外渠道标识
 * @property domain 邮箱域名
 */
class Catchmail(private val channel: String, private val domain: String) : Provider {

    private val headers = mapOf(
        "Accept" to "application/json",
        "Referer" to "https://catchmail.io/",
        "Origin" to "https://catchmail.io",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val selected = pickDomain(domain)
        val email = "${ProviderUtil.randomLocalSdk()}@$selected"
        val resp = ProviderUtil.httpGet("$API_BASE/mailbox?address=${ProviderUtil.urlEncode(email)}", headers)
        resp.ensureSuccess()
        return EmailInfo(email = email, channel = channel)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("catchmail: empty email")
        val resp = ProviderUtil.httpGet("$API_BASE/mailbox?address=${ProviderUtil.urlEncode(address)}", headers)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val rows = ProviderUtil.arr(data, "messages") ?: return emptyList()
        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val messageId = ProviderUtil.str(item, "id").trim()
            if (messageId.isEmpty()) continue
            val detail = fetchMessage(messageId, address)
            if (detail != null) {
                result.add(Normalize.fromMap(flattenDetail(detail, address), address))
            } else {
                result.add(
                    Normalize.fromMap(
                        mapOf(
                            "id" to messageId,
                            "from" to cleanAddress(ProviderUtil.str(item, "from")),
                            "to" to ProviderUtil.firstNonEmpty(ProviderUtil.str(item, "mailbox"), address),
                            "subject" to ProviderUtil.str(item, "subject"),
                            "date" to ProviderUtil.str(item, "date"),
                        ),
                        address,
                    ),
                )
            }
        }
        return result
    }

    private suspend fun fetchMessage(messageId: String, email: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$API_BASE/message/${ProviderUtil.urlEncode(messageId)}?mailbox=${ProviderUtil.urlEncode(email)}",
                headers,
            )
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }

    private fun flattenDetail(raw: JsonObject, recipient: String): Map<String, Any?> {
        val body = raw["body"] as? JsonObject
        val to = cleanAddress(ProviderUtil.str(raw, "to"))
        return mapOf(
            "id" to ProviderUtil.str(raw, "id"),
            "from" to cleanAddress(ProviderUtil.str(raw, "from")),
            "to" to to.ifEmpty { recipient },
            "subject" to ProviderUtil.str(raw, "subject"),
            "text" to (body?.let { ProviderUtil.str(it, "text") } ?: ""),
            "html" to (body?.let { ProviderUtil.str(it, "html") } ?: ""),
            "date" to ProviderUtil.str(raw, "date"),
        )
    }

    private fun cleanAddress(value: String?): String {
        if (value.isNullOrEmpty()) return ""
        angleAddr.find(value)?.let { return it.groupValues[1].trim() }
        return value.trim()
    }

    private fun pickDomain(preferred: String?): String {
        var p = (preferred ?: "").trim()
        if (p.startsWith("@")) p = p.substring(1)
        p = p.lowercase()
        if (p.isNotEmpty() && p in DOMAINS) return p
        return DEFAULT_DOMAIN
    }

    companion object {
        private const val API_BASE = "https://api.catchmail.io/api/v1"
        private const val DEFAULT_DOMAIN = "catchmail.io"
        private val DOMAINS = setOf("catchmail.io", "mailistry.com", "zeppost.com")
        private val angleAddr = Regex("<([^>]+)>")
    }
}
