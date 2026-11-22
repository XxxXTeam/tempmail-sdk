package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject

/**
 * GeniusGSM 渠道（tempmail.geniusgsm.com）。
 *
 * 创建收件箱返回内部 ID；收件箱列表响应已包含 body_text / body_html。
 *
 * @property channel 对外渠道标识
 */
class GeniusGSM(private val channel: String) : Provider {

    private val headers = mapOf(
        "Accept" to "application/json",
        "Content-Type" to "application/json",
    )

    private fun rows(node: kotlinx.serialization.json.JsonElement?): List<kotlinx.serialization.json.JsonElement> {
        return when (node) {
            is JsonArray -> node.toList()
            is JsonObject -> (node["results"] as? JsonArray)?.toList() ?: emptyList()
            else -> emptyList()
        }
    }

    private fun fromValue(raw: JsonObject): String {
        val name = ProviderUtil.str(raw, "from_name")
        val address = ProviderUtil.str(raw, "from_email")
        return if (name.isEmpty() || address.isEmpty()) address else "$name <$address>"
    }

    /** 选择激活域名并创建收件箱。 */
    override suspend fun generate(): EmailInfo {
        val domainResp = ProviderUtil.httpGet("$API_BASE/domains/", headers)
        domainResp.ensureSuccess()
        val active = rows(ProviderUtil.parse(domainResp.body))
            .filterIsInstance<JsonObject>()
            .firstOrNull { it["is_active"]?.toString() == "true" }
            ?: throw RuntimeException("geniusgsm: no active domain")
        val domainId = ProviderUtil.str(active, "id")

        val resp = ProviderUtil.httpPost(
            "$API_BASE/inboxes/create_inbox/",
            body = """{"domain_id":$domainId,"ttl":3600}""",
            contentType = "application/json",
            headers = mapOf("Accept" to "application/json"),
        )
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val id = ProviderUtil.str(data, "id").trim()
        val email = ProviderUtil.str(data, "email").trim()
        if (id.isEmpty() || email.isEmpty()) {
            throw RuntimeException("geniusgsm: incomplete creation response")
        }
        return EmailInfo(
            email = email,
            channel = channel,
            token = id,
            expiresAt = ProviderUtil.str(data, "expires_at"),
            createdAt = ProviderUtil.str(data, "created_at"),
        )
    }

    /** 获取 GeniusGSM 邮件，列表响应已包含完整正文。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val inboxId = info.token.trim()
        if (inboxId.isEmpty()) throw RuntimeException("geniusgsm: empty inbox id")
        val resp = ProviderUtil.httpGet(
            "$API_BASE/inboxes/${ProviderUtil.urlEncode(inboxId)}/emails/",
            headers,
        )
        resp.ensureSuccess()
        return rows(ProviderUtil.parse(resp.body)).filterIsInstance<JsonObject>().map { item ->
            Normalize.fromMap(
                mapOf(
                    "id" to ProviderUtil.str(item, "id"),
                    "from" to fromValue(item),
                    "to" to ProviderUtil.firstNonEmpty(ProviderUtil.str(item, "inbox_email"), info.email),
                    "subject" to ProviderUtil.str(item, "subject"),
                    "text" to ProviderUtil.str(item, "body_text"),
                    "html" to ProviderUtil.str(item, "body_html"),
                    "date" to ProviderUtil.str(item, "received_at"),
                    "read" to ProviderUtil.str(item, "is_read"),
                ),
                info.email,
            )
        }
    }

    private companion object {
        const val API_BASE = "https://tempmail.geniusgsm.com/api"
    }
}
