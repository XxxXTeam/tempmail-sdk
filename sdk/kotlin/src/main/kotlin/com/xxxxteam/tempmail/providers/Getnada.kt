package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * GetNada 渠道及其众多域名别名（getnada.net/api）。
 *
 * @property channel 对外渠道标识
 * @property domain 指定域名别名，可为空
 */
class Getnada(private val channel: String, private val domain: String) : Provider {

    /** 创建邮箱。 */
    override suspend fun generate(): EmailInfo {
        val selected = pickDomain(domain)
        val requested = "${ProviderUtil.randomLocalSdk()}@$selected"
        val payload = buildJsonObject { put("email", requested) }.toString()
        val resp = ProviderUtil.httpPost("$API_BASE/inbox/open", payload, "application/json", HEADERS_JSON)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body) ?: throw RuntimeException("getnada: invalid open response")
        val token = ProviderUtil.str(data, "token")
        var email = ProviderUtil.str(data, "recipient")
        if (email.isEmpty()) email = requested
        if (token.isEmpty() || !email.contains("@")) throw RuntimeException("getnada: invalid open response")
        val expiresAt = (data["activeUntil"] as? JsonPrimitive)?.longOrNull?.toString() ?: ""
        return EmailInfo(email = email, channel = channel, token = token, expiresAt = expiresAt)
    }

    /** 获取邮件列表，逐封拉取详情。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val auth = info.token.trim()
        if (auth.isEmpty()) throw RuntimeException("getnada: empty token")
        val resp = ProviderUtil.httpGet("$API_BASE/inbox/messages?token=${ProviderUtil.urlEncode(auth)}", HEADERS_JSON)
        resp.ensureSuccess()
        val rows = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "messages") ?: return emptyList()
        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val id = ProviderUtil.str(item, "id")
            var src = item
            if (id.isNotEmpty()) {
                try {
                    val dr = ProviderUtil.httpGet(
                        "$API_BASE/inbox/message?id=${ProviderUtil.urlEncode(id)}&token=${ProviderUtil.urlEncode(auth)}",
                        HEADERS_JSON,
                    )
                    if (dr.isOk) {
                        val det = ProviderUtil.parseObject(dr.body)
                        (det?.get("message") as? JsonObject)?.let { src = it }
                    }
                } catch (_: Exception) {
                }
            }
            result.add(Normalize.fromMap(flatten(src, info.email), info.email))
        }
        return result
    }

    private fun flatten(raw: JsonObject, recipient: String): Map<String, Any?> {
        val to = ProviderUtil.str(raw, "to")
        val dict = linkedMapOf<String, Any?>()
        raw.forEach { (k, v) -> dict[k] = v }
        dict["from"] = ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "from_addr"), ProviderUtil.str(raw, "from"))
        dict["to"] = to.ifEmpty { recipient }
        dict["text"] = ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "text_plain"), ProviderUtil.str(raw, "text"))
        dict["html"] = ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "html_sanitized"), ProviderUtil.str(raw, "html"))
        if (raw.containsKey("read_at")) dict["read"] = ProviderUtil.str(raw, "read_at").isNotEmpty()
        return dict
    }

    private suspend fun pickDomain(preferred: String?): String {
        val resp = ProviderUtil.httpGet("$API_BASE/public/domains", HEADERS_JSON)
        resp.ensureSuccess()
        val arr = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "domains")
        val domains = mutableListOf<String>()
        arr?.filterIsInstance<JsonPrimitive>()?.forEach {
            val s = it.content.trim().lowercase()
            if (s.isNotEmpty() && s.length <= 253 && s.contains(".")) domains.add(s)
        }
        var want = (preferred ?: "").trim()
        if (want.startsWith("@")) want = want.substring(1)
        want = want.lowercase()
        if (want.isNotEmpty()) {
            domains.firstOrNull { it == want }?.let { return it }
            throw RuntimeException("getnada: domain not available: $want")
        }
        domains.firstOrNull { it == "getnada.net" }?.let { return it }
        if (domains.isNotEmpty()) return domains[0]
        throw RuntimeException("getnada: no domain available")
    }

    companion object {
        private const val API_BASE = "https://getnada.net/api"
        private val HEADERS_JSON = mapOf("Accept" to "application/json")
    }
}
