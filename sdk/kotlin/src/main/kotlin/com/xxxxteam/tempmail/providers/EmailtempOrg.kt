package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * emailtemp.org 渠道实现 — https://emailtemp.org
 *
 * Laravel + CSRF，GET /en 获取 session cookie + CSRF token，
 * POST /messages 获取邮箱地址和邮件列表，GET /view/{id} 获取正文。
 * token 使用 base64 编码，前缀 "eto1:"。
 */
object EmailtempOrg : Provider {

    private const val BASE_URL = "https://emailtemp.org"
    private const val TOK_PREFIX = "eto1:"

    private fun ajaxHeaders(csrf: String, cookie: String): Map<String, String> {
        val h = linkedMapOf(
            "User-Agent" to "Mozilla/5.0",
            "Accept" to "application/json, text/javascript, */*; q=0.01",
            "X-Requested-With" to "XMLHttpRequest",
            "Referer" to "$BASE_URL/en",
            "Content-Type" to "application/x-www-form-urlencoded",
            "X-CSRF-TOKEN" to csrf,
        )
        if (cookie.isNotEmpty()) h["Cookie"] = cookie
        return h
    }

    /** 打包 CSRF + cookie 为 base64 token。 */
    private fun encodeToken(csrf: String, cookie: String): String {
        val raw = buildJsonObject {
            put("t", csrf)
            put("c", cookie)
        }.toString()
        return TOK_PREFIX + ProviderUtil.base64Encode(raw.toByteArray(Charsets.UTF_8))
    }

    /** 从 base64 token 解包，返回 (csrf, cookie)。 */
    private fun decodeToken(token: String?): Pair<String, String> {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw RuntimeException("emailtemp-org: 无效的 token")
        }
        val raw = String(ProviderUtil.base64Decode(token.substring(TOK_PREFIX.length)), Charsets.UTF_8)
        val obj = ProviderUtil.parseObject(raw) ?: throw RuntimeException("emailtemp-org: 无效的 token")
        return ProviderUtil.str(obj, "t") to ProviderUtil.str(obj, "c")
    }

    /** POST /messages 获取响应 JSON。 */
    private suspend fun postMessages(csrf: String, cookie: String): JsonObject {
        val resp = ProviderUtil.httpPost(
            "$BASE_URL/messages",
            "_token=${ProviderUtil.urlEncode(csrf)}&captcha=",
            "application/x-www-form-urlencoded",
            ajaxHeaders(csrf, cookie),
        )
        resp.ensureSuccess()
        return ProviderUtil.parseObject(resp.body)
            ?: throw RuntimeException("emailtemp-org: 响应 JSON 解析失败")
    }

    /** 创建 emailtemp.org 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val bh = mapOf("User-Agent" to "Mozilla/5.0", "Accept" to "text/html")
        val r1 = ProviderUtil.httpGet("$BASE_URL/en", bh)
        r1.ensureSuccess()
        val cookie = ProviderUtil.extractAllCookies(r1)
        val csrf = ProviderUtil.extractCsrfToken(r1.body)
        if (csrf.isEmpty()) throw RuntimeException("emailtemp-org: 未能提取 CSRF token")
        val data = postMessages(csrf, cookie)
        val mailbox = ProviderUtil.str(data, "mailbox").trim()
        if (mailbox.isEmpty() || !mailbox.contains("@")) {
            throw RuntimeException("emailtemp-org: 邮箱地址无效")
        }
        return EmailInfo(email = mailbox, channel = "emailtemp-org", token = encodeToken(csrf, cookie))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("emailtemp-org: 邮箱地址为空")
        val (csrf, cookie) = decodeToken(info.token)
        val data = postMessages(csrf, cookie)
        val msgs = ProviderUtil.arr(data, "messages") ?: return emptyList()
        val out = ArrayList<Email>(msgs.size)
        for (item in msgs) {
            val msg = item as? JsonObject ?: continue
            val mid = ProviderUtil.str(msg, "id").trim()
            if (mid.isEmpty()) continue
            val fromEmail = ProviderUtil.str(msg, "from_email")
            val fromName = ProviderUtil.str(msg, "from")
            val from = if (fromName.isNotEmpty() && fromName != fromEmail) "$fromName <$fromEmail>" else fromEmail
            val row = mapOf(
                "id" to mid,
                "from" to from,
                "to" to address,
                "subject" to ProviderUtil.str(msg, "subject"),
                "html" to fetchView(cookie, mid),
            )
            out.add(Normalize.fromMap(row, address))
        }
        return out
    }

    /** 拉取单封邮件 HTML 正文，失败返回空串。 */
    private suspend fun fetchView(cookie: String, mid: String): String {
        if (mid.isEmpty()) return ""
        return try {
            val h = linkedMapOf(
                "User-Agent" to "Mozilla/5.0",
                "Referer" to "$BASE_URL/en",
            )
            if (cookie.isNotEmpty()) h["Cookie"] = cookie
            val resp = ProviderUtil.httpGet("$BASE_URL/view/${ProviderUtil.urlEncode(mid)}", h)
            if (resp.isOk) resp.body else ""
        } catch (_: Exception) {
            ""
        }
    }
}
