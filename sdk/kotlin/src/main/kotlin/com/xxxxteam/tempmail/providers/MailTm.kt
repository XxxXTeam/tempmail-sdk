package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.net.URI

/**
 * mail.tm 协议家族实现（mail.tm / duckmail 等共用 Hydra REST API，仅 baseUrl 不同）。
 *
 * 流程：获取域名 → 随机邮箱/密码 → 创建账号 → 换取 Bearer Token。
 *
 * @property channel 对外渠道标识
 * @property baseUrl API 基地址
 */
class MailTm(private val channel: String, private val baseUrl: String) : Provider {

    private fun baseHeaders(): Map<String, String> = mapOf(
        "Accept" to "application/json",
        "Origin" to originOf(baseUrl),
    )

    private fun originOf(url: String): String = try {
        val u = URI.create(url)
        "${u.scheme}://${u.host}"
    } catch (_: Exception) {
        url
    }

    /** 拉取激活域名列表。 */
    private suspend fun getDomains(): List<String> {
        val resp = ProviderUtil.httpGet("$baseUrl/domains", baseHeaders())
        resp.ensureSuccess()
        val parsed = ProviderUtil.parse(resp.body)
        val members = parsed as? JsonArray ?: (parsed as? JsonObject)?.get("hydra:member") as? JsonArray
        val domains = mutableListOf<String>()
        members?.filterIsInstance<JsonObject>()?.forEach { o ->
            val active = (o["isActive"] as? JsonPrimitive)?.booleanOrNull ?: false
            val domain = ProviderUtil.str(o, "domain")
            if (active && domain.isNotEmpty()) domains.add(domain)
        }
        return domains
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val domains = getDomains()
        if (domains.isEmpty()) throw RuntimeException("$channel: 无可用域名")
        val address = "${ProviderUtil.randomString(12)}@${domains.random()}"
        val password = ProviderUtil.randomString(16)
        val payload = buildJsonObject {
            put("address", address)
            put("password", password)
        }.toString()

        val accResp = ProviderUtil.httpPost("$baseUrl/accounts", payload, "application/ld+json", baseHeaders())
        accResp.ensureSuccess()
        val account = ProviderUtil.parseObject(accResp.body)

        val tokResp = ProviderUtil.httpPost("$baseUrl/token", payload, "application/json", baseHeaders())
        tokResp.ensureSuccess()
        val token = ProviderUtil.str(ProviderUtil.parseObject(tokResp.body), "token")

        return EmailInfo(
            email = address,
            channel = channel,
            token = token,
            createdAt = ProviderUtil.str(account, "createdAt"),
        )
    }

    /** 获取邮件列表，逐封拉取详情。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val headers = baseHeaders() + ("Authorization" to "Bearer ${info.token}")
        val resp = ProviderUtil.httpGet("$baseUrl/messages", headers)
        resp.ensureSuccess()
        val parsed = ProviderUtil.parse(resp.body)
        val messages = parsed as? JsonArray ?: (parsed as? JsonObject)?.get("hydra:member") as? JsonArray
        ?: return emptyList()

        val result = mutableListOf<Email>()
        for (mo in messages.filterIsInstance<JsonObject>()) {
            val id = ProviderUtil.str(mo, "id")
            var detail = mo
            if (id.isNotEmpty()) {
                try {
                    val dr = ProviderUtil.httpGet("$baseUrl/messages/$id", headers)
                    if (dr.isOk) {
                        ProviderUtil.parseObject(dr.body)?.let { detail = it }
                    }
                } catch (_: Exception) {
                }
            }
            result.add(Normalize.fromMap(flatten(detail, info.email), info.email))
        }
        return result
    }

    /** 扁平化单封邮件（from/to 为对象/数组，html 为数组）。 */
    private fun flatten(msg: JsonObject, recipient: String): Map<String, Any?> {
        val from = when (val f = msg["from"]) {
            is JsonObject -> ProviderUtil.str(f, "address")
            is JsonPrimitive -> f.content
            else -> ""
        }
        var to = recipient
        (msg["to"] as? JsonArray)?.firstOrNull()?.let { first ->
            if (first is JsonObject) {
                val addr = ProviderUtil.str(first, "address")
                if (addr.isNotEmpty()) to = addr
            }
        }
        val html = when (val h = msg["html"]) {
            is JsonArray -> h.filterIsInstance<JsonPrimitive>().joinToString("") { it.content }
            else -> ProviderUtil.str(msg, "html")
        }
        return mapOf(
            "id" to ProviderUtil.str(msg, "id"),
            "from" to from,
            "to" to to,
            "subject" to ProviderUtil.str(msg, "subject"),
            "text" to ProviderUtil.str(msg, "text"),
            "html" to html,
            "createdAt" to ProviderUtil.str(msg, "createdAt"),
        )
    }
}
