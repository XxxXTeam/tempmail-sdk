package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.*
import kotlinx.serialization.json.*

/**
 * mail.cx 匿名 Web API 渠道（mail.cx/v1）。ddker.com 复用。
 */
class MailCx(private val channel: String, private val domain: String) : Provider {

    private val baseUrl = "https://mail.cx"

    private fun buildHeaders(clientId: String) = mapOf(
        "Accept" to "application/json",
        "Origin" to baseUrl,
        "Referer" to "$baseUrl/",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "X-Client-ID" to clientId,
    )

    override suspend fun generate(): EmailInfo {
        val clientId = "tempmail-sdk-" + ProviderUtil.randomString(16)
        val resp = ProviderUtil.httpGet("$baseUrl/v1/config", buildHeaders(clientId))
        resp.ensureSuccess()
        val config = ProviderUtil.parseObject(resp.body)
        val pickedDomain = pickDomain(config, domain)
        val email = ProviderUtil.randomString(12) + "@" + pickedDomain
        return EmailInfo(email = email, channel = channel, token = clientId)
    }

    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val clientId = info.token.ifEmpty { "" }
        val email = info.email
        val resp = ProviderUtil.httpGet(
            "$baseUrl/v1/inbox/${ProviderUtil.urlEncode(email)}",
            buildHeaders(clientId),
        )
        if (resp.statusCode == 204) return emptyList()
        resp.ensureSuccess()
        val root = ProviderUtil.parse(resp.body)
        val rows = (root as? JsonObject)?.get("emails") as? JsonArray ?: return emptyList()
        return rows.mapNotNull { row ->
            val ro = row as? JsonObject ?: return@mapNotNull null
            val id = ProviderUtil.str(ro, "id")
            var raw = ro
            if (id.isNotEmpty()) {
                try {
                    val dr = ProviderUtil.httpGet(
                        "$baseUrl/v1/email/${ProviderUtil.urlEncode(id)}",
                        buildHeaders(clientId),
                    )
                    if (dr.isOk) {
                        val det = ProviderUtil.parseObject(dr.body)
                        if (det != null) raw = det
                    }
                } catch (_: Exception) {}
            }
            Normalize.fromJson(raw, email)
        }
    }

    private fun pickDomain(config: JsonObject?, preferred: String?): String {
        val items = config?.get("system_domains") as? JsonArray
        val domains = mutableListOf<String>()
        items?.forEach { el ->
            val obj = el as? JsonObject
            val d = ProviderUtil.str(obj, "domain").trim()
            if (d.isNotEmpty()) domains.add(d)
        }
        if (domains.isEmpty()) throw RuntimeException("mail-cx: no system domains")
        val want = preferred?.trim()?.removePrefix("@")?.lowercase() ?: ""
        if (want.isNotEmpty()) {
            domains.find { it.lowercase() == want }?.let { return it }
        }
        items?.forEach { el ->
            val obj = el as? JsonObject ?: return@forEach
            if (ProviderUtil.bool(obj, "default")) {
                val d = ProviderUtil.str(obj, "domain").trim()
                if (d.isNotEmpty()) return d
            }
        }
        return domains[0]
    }
}
