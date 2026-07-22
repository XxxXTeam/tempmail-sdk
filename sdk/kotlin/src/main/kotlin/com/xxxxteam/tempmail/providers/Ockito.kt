package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import com.xxxxteam.tempmail.providers.ProviderUtil.firstNonEmpty
import com.xxxxteam.tempmail.providers.ProviderUtil.str
import kotlinx.serialization.json.*

/**
 * Ockito 渠道（ockito.com）。
 *
 * 两段式认证：/gtoken 获取 access/refresh token，后续请求需 Bearer 认证。
 * Token 以 JSON 格式序列化存储（含 access_token 和 refresh_token）。
 */
object Ockito : Provider {

    private const val BASE_URL = "https://ockito.com/web-api"
    private val DEFAULT_HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val loginResp = ProviderUtil.httpPost(
            "$BASE_URL/gtoken", "{}", "application/json", DEFAULT_HEADERS)
        loginResp.ensureSuccess()
        val login = ProviderUtil.parseObject(loginResp.body)
            ?: throw RuntimeException("ockito: 无效的 token 响应")
        val accessToken = str(login, "access_token").trim()
        val refreshToken = str(login, "refresh_token").trim()
        if (accessToken.isEmpty() || refreshToken.isEmpty()) {
            throw RuntimeException("ockito: 无效的 token 响应")
        }

        val authHeaders = DEFAULT_HEADERS + ("Authorization" to "Bearer $accessToken")
        val emailResp = ProviderUtil.httpGet("$BASE_URL/email", authHeaders)
        emailResp.ensureSuccess()
        val emailData = ProviderUtil.parseObject(emailResp.body)
            ?: throw RuntimeException("ockito: 无效的邮箱响应")
        val emailEl = emailData["email"]
        val email = when (emailEl) {
            is JsonPrimitive -> emailEl.content.trim()
            is JsonObject -> str(emailEl, "email").trim()
            else -> ""
        }
        if (email.isEmpty() || !email.contains("@")) {
            throw RuntimeException("ockito: 无效的邮箱响应")
        }

        return EmailInfo(email = email, channel = "ockito", token = packToken(accessToken, refreshToken))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        if (info.token.isEmpty()) throw RuntimeException("ockito: 缺少 token")
        val (accessToken, refreshToken) = unpackToken(info.token)
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("ockito: 缺少邮箱地址")

        val data = fetchBearerJson(
            "/retrieve/${ProviderUtil.urlEncode(address)}/emails", accessToken, refreshToken)
        val rows = ProviderUtil.arr(data, "inbox") ?: return emptyList()

        val result = ArrayList<Email>()
        for (item in rows) {
            val row = item as? JsonObject ?: continue
            val uid = str(row, "uid").trim()
            if (uid.isEmpty()) {
                result.add(Normalize.fromMap(flattenInboxRow(row, address), address))
                continue
            }
            try {
                val detail = fetchBearerJson(
                    "/retrieve/${ProviderUtil.urlEncode(address)}/${ProviderUtil.urlEncode(uid)}",
                    accessToken, refreshToken)
                result.add(Normalize.fromMap(flattenMessage(detail, address), address))
            } catch (_: Exception) {
                result.add(Normalize.fromMap(flattenInboxRow(row, address), address))
            }
        }
        return result
    }

    /** 将 access/refresh token 打包为 JSON 字符串。 */
    private fun packToken(accessToken: String, refreshToken: String): String =
        buildJsonObject {
            put("access_token", accessToken)
            put("refresh_token", refreshToken)
        }.toString()

    /** 从 JSON 字符串解包出 (access, refresh)。 */
    private fun unpackToken(token: String): Pair<String, String> {
        val value = token.trim()
        if (value.isEmpty() || !value.startsWith("{")) {
            throw RuntimeException("ockito: 无效的会话 token")
        }
        val obj = ProviderUtil.parseObject(value)
            ?: throw RuntimeException("ockito: 无效的会话 token")
        val access = str(obj, "access_token").trim()
        val refresh = str(obj, "refresh_token").trim()
        if (access.isEmpty() || refresh.isEmpty()) {
            throw RuntimeException("ockito: 无效的会话 token")
        }
        return access to refresh
    }

    /** 使用 refresh token 换取新的 access token。 */
    private suspend fun refreshAccessToken(refreshToken: String): String {
        val headers = DEFAULT_HEADERS + mapOf(
            "Authorization" to "Bearer $refreshToken",
            "X-PASSTHROUGH" to "Y",
        )
        val resp = ProviderUtil.httpPost("$BASE_URL/grefresh", "{}", "application/json", headers)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
            ?: throw RuntimeException("ockito: 刷新 token 失败")
        val newAccess = str(data, "access_token").trim()
        if (newAccess.isEmpty()) throw RuntimeException("ockito: 刷新 token 响应无效")
        return newAccess
    }

    /** 带 Bearer 认证的 GET，遇 401 时刷新 token 后重试一次。 */
    private suspend fun fetchBearerJson(
        path: String, accessToken: String, refreshToken: String,
    ): JsonObject {
        val resp = ProviderUtil.httpGet(
            "$BASE_URL$path", DEFAULT_HEADERS + ("Authorization" to "Bearer $accessToken"))
        if (resp.statusCode != 401) {
            resp.ensureSuccess()
            return ProviderUtil.parseObject(resp.body) ?: JsonObject(emptyMap())
        }
        val refreshed = refreshAccessToken(refreshToken)
        val retry = ProviderUtil.httpGet(
            "$BASE_URL$path", DEFAULT_HEADERS + ("Authorization" to "Bearer $refreshed"))
        retry.ensureSuccess()
        return ProviderUtil.parseObject(retry.body) ?: JsonObject(emptyMap())
    }

    /** 扁平化收件箱列表行。 */
    private fun flattenInboxRow(raw: JsonObject, recipient: String): Map<String, Any?> = mapOf(
        "id" to str(raw, "uid"),
        "from" to str(raw, "sender"),
        "to" to recipient,
        "subject" to str(raw, "subject"),
        "text" to str(raw, "snippet"),
        "html" to str(raw, "html"),
        "date" to str(raw, "timestamp"),
    )

    /** 扁平化邮件详情（可能嵌套在 obj 字段内）。 */
    private fun flattenMessage(raw: JsonObject, recipient: String): Map<String, Any?> {
        val obj = (raw["obj"] as? JsonObject) ?: raw
        val to = firstNonEmpty(str(obj, "to"), str(obj, "To"))
        return mapOf(
            "id" to str(raw, "uid"),
            "from" to firstNonEmpty(
                str(obj, "sender_email"), str(obj, "SenderEmail"),
                str(obj, "from_"), str(obj, "From"),
                str(obj, "from"), str(obj, "sender_name"), str(obj, "SenderName")),
            "to" to to.ifEmpty { recipient },
            "subject" to firstNonEmpty(str(obj, "subject"), str(obj, "Subject")),
            "text" to str(obj, "text"),
            "html" to str(obj, "html"),
            "date" to firstNonEmpty(str(obj, "date"), str(obj, "Date")),
        )
    }
}
