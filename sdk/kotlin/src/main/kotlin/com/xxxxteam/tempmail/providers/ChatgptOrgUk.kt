package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * mail.chatgpt.org.uk 渠道 — https://mail.chatgpt.org.uk
 *
 * GET /api/domains/public 获取域名 → POST /api/inbox-token 创建收件箱
 * → GET /api/emails?email=xxx 获取邮件。token 存复合 JSON {gmSid, inbox}。
 */
object ChatgptOrgUk : Provider {

    private const val BASE_URL = "https://mail.chatgpt.org.uk/api"

    private fun defaultHeaders(): MutableMap<String, String> = mutableMapOf(
        "User-Agent" to "Mozilla/5.0",
        "Accept" to "*/*",
        "Referer" to "https://mail.chatgpt.org.uk/zh/",
        "Origin" to "https://mail.chatgpt.org.uk",
    )

    /** 从 Set-Cookie 头集合提取 gm_sid 值。 */
    private fun extractGmSid(cookies: List<String>): String {
        for (c in cookies) {
            val idx = c.indexOf("gm_sid=")
            if (idx >= 0) {
                val seg = c.substring(idx + 7)
                val semi = seg.indexOf(';')
                return if (semi < 0) seg else seg.substring(0, semi)
            }
        }
        return ""
    }

    /** 创建 chatgpt-org-uk 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val domResp = ProviderUtil.httpGet("$BASE_URL/domains/public", defaultHeaders())
        domResp.ensureSuccess()
        val domBody = ProviderUtil.parseObject(domResp.body)
            ?: throw RuntimeException("chatgpt-org-uk: 获取域名失败")
        if (!domBody.containsKey("success")) throw RuntimeException("chatgpt-org-uk: 获取域名失败")
        val domData = domBody["data"] as? JsonObject
        val domains = ProviderUtil.arr(domData, "domains")
        if (domains == null || domains.isEmpty()) throw RuntimeException("chatgpt-org-uk: 无可用域名")

        val active = ArrayList<String>()
        for (d in domains) {
            val dObj = d as? JsonObject ?: continue
            if ((dObj["is_active"] as? JsonPrimitive)?.content?.toIntOrNull() == 1) {
                active.add(ProviderUtil.str(dObj, "domain_name"))
            }
        }
        if (active.isEmpty()) throw RuntimeException("chatgpt-org-uk: 无可用域名")
        val email = ProviderUtil.randomString(10) + "@" + active.random()

        val h = defaultHeaders()
        h["Content-Type"] = "application/json"
        val payload = buildJsonObject { put("email", email) }.toString()
        val tokResp = ProviderUtil.httpPost("$BASE_URL/inbox-token", payload, "application/json", h)
        tokResp.ensureSuccess()
        val gmSid = extractGmSid(tokResp.setCookies)
        val auth = ProviderUtil.parseObject(tokResp.body)?.get("auth") as? JsonObject
        val inboxToken = ProviderUtil.str(auth, "token")
        if (inboxToken.isEmpty()) throw RuntimeException("chatgpt-org-uk: 响应缺少 token")
        val packed = buildJsonObject { put("gmSid", gmSid); put("inbox", inboxToken) }.toString()
        return EmailInfo(email = email, channel = "chatgpt-org-uk", token = packed)
    }
    /** 获取邮件列表，token 为复合 JSON {gmSid, inbox}。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token
        if (token.isBlank()) throw RuntimeException("chatgpt-org-uk: token 不能为空")
        val email = info.email
        val tok = ProviderUtil.parseObject(token)
        var gmSid = ProviderUtil.str(tok, "gmSid")
        var inbox = if (tok != null) ProviderUtil.str(tok, "inbox") else token

        // gmSid 为空则重新创建 inbox
        if (gmSid.isEmpty()) {
            val h2 = defaultHeaders()
            h2["Content-Type"] = "application/json"
            val r2 = ProviderUtil.httpPost("$BASE_URL/inbox-token",
                buildJsonObject { put("email", email) }.toString(), "application/json", h2)
            r2.ensureSuccess()
            val a2 = ProviderUtil.parseObject(r2.body)?.get("auth") as? JsonObject
            inbox = ProviderUtil.str(a2, "token")
            gmSid = extractGmSid(r2.setCookies)
        }

        val h = defaultHeaders()
        h["x-inbox-token"] = inbox
        if (gmSid.isNotEmpty()) h["Cookie"] = "gm_sid=$gmSid"
        val resp = ProviderUtil.httpGet("$BASE_URL/emails?email=" + ProviderUtil.urlEncode(email), h)
        resp.ensureSuccess()
        val body = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        if (!body.containsKey("success")) return emptyList()
        val data = body["data"] as? JsonObject
        val emails = ProviderUtil.arr(data, "emails") ?: return emptyList()
        val out = ArrayList<Email>(emails.size)
        for (item in emails) {
            val obj = item as? JsonObject ?: continue
            out.add(Normalize.fromJson(obj, email))
        }
        return out
    }
}
