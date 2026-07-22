package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.util.Base64

/**
 * mail.zhujump.com 固定域渠道。
 *
 * 通过注册账号、登录会话后创建固定域邮箱，再通过邮箱 ID 拉取邮件列表。
 * 渠道别名：jqkjqk-xyz / lyhlevi-com
 *
 * @property channel 对外渠道标识
 * @property domain 邮箱域名
 */
class Zhujump(
    private val channel: String,
    private val domain: String,
    private val baseUrl: String = BASE_URL,
    private val expiryTime: Long = DEFAULT_EXPIRY_TIME,
) : Provider {

    /** 创建临时邮箱（实例 baseUrl 默认 mail.zhujump.com）。 */
    override suspend fun generate(): EmailInfo {
        val baseUrl = this.baseUrl.trimEnd('/')
        val headers = baseHeaders(baseUrl)

        val username = "sdk${ProviderUtil.randomString(10)}"
        val password = "Sdk!${ProviderUtil.randomString(12)}A1"

        // 1. 注册账号
        val regBody = buildJsonObject {
            put("username", username)
            put("password", password)
            put("turnstileToken", "")
        }.toString()
        val regResp = ProviderUtil.httpPost("$baseUrl/api/auth/register", regBody, "application/json", headers)
        regResp.ensureSuccess()

        // 2. 获取 CSRF token
        val csrfResp = ProviderUtil.httpGet("$baseUrl/api/auth/csrf", headers)
        csrfResp.ensureSuccess()
        val csrf = ProviderUtil.str(ProviderUtil.parseObject(csrfResp.body), "csrfToken").trim()
        if (csrf.isEmpty()) throw RuntimeException("zhujump: csrf token missing")

        // 3. 登录（form-urlencoded，不跟随重定向以捕获 Set-Cookie）
        val loginBody = "username=${ProviderUtil.urlEncode(username)}" +
            "&password=${ProviderUtil.urlEncode(password)}" +
            "&turnstileToken=" +
            "&redirect=false" +
            "&csrfToken=${ProviderUtil.urlEncode(csrf)}" +
            "&callbackUrl=${ProviderUtil.urlEncode("$baseUrl/zh-CN/login")}"
        val loginHeaders = headers + mapOf(
            "x-auth-return-redirect" to "1",
            "Content-Type" to "application/x-www-form-urlencoded",
        )
        val loginResp = ProviderUtil.httpPost(
            "$baseUrl/api/auth/callback/credentials?",
            loginBody, "application/x-www-form-urlencoded", loginHeaders, followRedirects = false,
        )

        // 收集 Cookie
        var cookie = collectCookies(regResp, csrfResp, loginResp)

        // 4. 验证 session
        val sessionResp = ProviderUtil.httpGet("$baseUrl/api/auth/session", headers + ("Cookie" to cookie))
        sessionResp.ensureSuccess()
        cookie = mergeCookies(cookie, sessionResp)

        // 5. 创建邮箱
        val createBody = buildJsonObject {
            put("name", "sdk${ProviderUtil.randomString(10)}")
            put("domain", domain)
            put("expiryTime", expiryTime)
        }.toString()
        val createResp = ProviderUtil.httpPost(
            "$baseUrl/api/emails/generate", createBody, "application/json", headers + ("Cookie" to cookie),
        )
        createResp.ensureSuccess()
        val created = ProviderUtil.parseObject(createResp.body) ?: throw RuntimeException("zhujump: invalid generate response")
        val email = ProviderUtil.str(created, "email").trim()
        val emailId = ProviderUtil.str(created, "id").trim()
        if (email.isEmpty() || emailId.isEmpty()) throw RuntimeException("zhujump: invalid generate response")

        return EmailInfo(email = email, channel = channel, token = encodeToken(cookie, emailId, baseUrl))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val session = decodeToken(info.token)
        val baseUrl = session.getValue("base_url")
        val emailId = session.getValue("email_id")
        val cookie = session.getValue("cookie")
        val headers = baseHeaders(baseUrl) + ("Cookie" to cookie)

        val resp = ProviderUtil.httpGet("$baseUrl/api/emails/${ProviderUtil.urlEncode(emailId)}", headers)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val rows = ProviderUtil.arr(data, "messages") ?: return emptyList()

        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            var source = item
            val messageId = ProviderUtil.str(item, "id").trim()
            if (messageId.isNotEmpty() &&
                ProviderUtil.str(item, "content").trim().isEmpty() &&
                ProviderUtil.str(item, "html").trim().isEmpty()
            ) {
                fetchDetail(baseUrl, cookie, emailId, messageId)?.let { source = mergeJson(item, it) }
            }
            result.add(Normalize.fromMap(flatten(source, info.email), info.email))
        }
        return result
    }

    private fun flatten(source: JsonObject, email: String): Map<String, Any?> {
        return mapOf(
            "id" to ProviderUtil.str(source, "id"),
            "from_address" to ProviderUtil.str(source, "from_address"),
            "to_address" to ProviderUtil.firstNonEmpty(ProviderUtil.str(source, "to_address"), email),
            "subject" to ProviderUtil.str(source, "subject"),
            "content" to ProviderUtil.str(source, "content"),
            "html" to ProviderUtil.str(source, "html"),
            "received_at" to ProviderUtil.firstNonEmpty(
                ProviderUtil.str(source, "received_at"), ProviderUtil.str(source, "sent_at"),
            ),
            "isRead" to false,
        )
    }

    private suspend fun fetchDetail(baseUrl: String, cookie: String, emailId: String, messageId: String): JsonObject? {
        return try {
            val headers = baseHeaders(baseUrl) + ("Cookie" to cookie)
            val resp = ProviderUtil.httpGet(
                "$baseUrl/api/emails/${ProviderUtil.urlEncode(emailId)}/${ProviderUtil.urlEncode(messageId)}", headers,
            )
            if (!resp.isOk) return null
            val data = ProviderUtil.parseObject(resp.body) ?: return null
            (data["message"] as? JsonObject) ?: data
        } catch (_: Exception) {
            null
        }
    }

    private fun mergeJson(base: JsonObject, overlay: JsonObject): JsonObject {
        val result = base.toMutableMap()
        for ((k, v) in overlay) {
            val cur = result[k]
            val empty = cur == null || cur is JsonNull ||
                (cur is JsonPrimitive && cur.isString && cur.content.isEmpty())
            if (empty) result[k] = v
        }
        return JsonObject(result)
    }

    private fun baseHeaders(baseUrl: String): Map<String, String> = mapOf(
        "Accept" to "application/json",
        "Origin" to baseUrl,
        "Referer" to "$baseUrl/zh-CN/login",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    )

    private fun encodeToken(cookie: String, emailId: String, baseUrl: String): String {
        val json = buildJsonObject {
            put("c", cookie)
            put("i", emailId)
            put("b", baseUrl)
        }.toString()
        return TOKEN_PREFIX + Base64.getEncoder().encodeToString(json.toByteArray(Charsets.UTF_8))
    }

    private fun decodeToken(token: String): Map<String, String> {
        if (!token.startsWith(TOKEN_PREFIX)) throw IllegalArgumentException("zhujump: invalid session token")
        try {
            val decoded = Base64.getDecoder().decode(token.substring(TOKEN_PREFIX.length))
            val data = ProviderUtil.parseObject(String(decoded, Charsets.UTF_8))
                ?: throw IllegalArgumentException("zhujump: invalid session token")
            val cookie = ProviderUtil.str(data, "c").trim()
            val emailId = ProviderUtil.str(data, "i").trim()
            if (cookie.isEmpty() || emailId.isEmpty()) throw IllegalArgumentException("zhujump: invalid session token")
            val baseUrl = ProviderUtil.str(data, "b").trim().ifEmpty { BASE_URL }.trimEnd('/')
            return mapOf("cookie" to cookie, "email_id" to emailId, "base_url" to baseUrl)
        } catch (e: IllegalArgumentException) {
            throw e
        } catch (e: Exception) {
            throw IllegalArgumentException("zhujump: invalid session token", e)
        }
    }

    /** 从多个响应的 Set-Cookie 收集并拼接为 Cookie 头字符串。 */
    private fun collectCookies(vararg responses: HttpResp): String {
        val cookies = LinkedHashMap<String, String>()
        for (resp in responses) parseSetCookies(resp.setCookies, cookies)
        return cookies.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }

    /** 将已有 cookie 字符串与新响应的 Set-Cookie 合并。 */
    private fun mergeCookies(existing: String, resp: HttpResp): String {
        val cookies = LinkedHashMap<String, String>()
        if (existing.isNotEmpty()) {
            for (pair in existing.split(";")) {
                val p = pair.trim()
                val eq = p.indexOf('=')
                if (eq > 0) cookies[p.substring(0, eq)] = p.substring(eq + 1)
            }
        }
        parseSetCookies(resp.setCookies, cookies)
        return cookies.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }

    private fun parseSetCookies(setCookies: List<String>, out: LinkedHashMap<String, String>) {
        for (sc in setCookies) {
            val nameValue = sc.split(";", limit = 2)[0].trim()
            val eq = nameValue.indexOf('=')
            if (eq > 0) out[nameValue.substring(0, eq)] = nameValue.substring(eq + 1)
        }
    }

    companion object {
        private const val BASE_URL = "https://mail.zhujump.com"
        private const val TOKEN_PREFIX = "zhj1:"
        private const val DEFAULT_EXPIRY_TIME = 60 * 60 * 1000L
    }
}
