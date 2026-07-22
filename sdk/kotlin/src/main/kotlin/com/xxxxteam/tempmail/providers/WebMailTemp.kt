package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.nio.charset.StandardCharsets
import java.util.Base64

/**
 * WebMailTemp 渠道（webmailtemp.com）。
 *
 * GET /api/create 创建，GET /api/check/{username} 收信。
 * token 使用 base64 编码，前缀 "wmt1:"，内含 username 和 cookie。
 */
object WebMailTemp : Provider {

    private const val BASE_URL = "https://webmailtemp.com"
    private const val TOK_PREFIX = "wmt1:"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 将 username + cookie 编码为 token。 */
    private fun encodeToken(username: String, cookie: String): String {
        val raw = buildJsonObject {
            put("u", username)
            put("c", cookie)
        }.toString()
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.toByteArray(StandardCharsets.UTF_8))
    }

    /** 解码 token，返回 (username, cookie)。 */
    private fun decodeToken(token: String): Pair<String, String> {
        if (!token.startsWith(TOK_PREFIX)) throw RuntimeException("webmailtemp: 无效的 token")
        val decoded = Base64.getDecoder().decode(token.substring(TOK_PREFIX.length))
        val obj = Http.json.parseToJsonElement(String(decoded, StandardCharsets.UTF_8)) as? JsonObject
            ?: throw RuntimeException("webmailtemp: token 数据无效")
        val username = jsonStr(obj, "u").trim()
        val cookie = jsonStr(obj, "c").trim()
        if (username.isEmpty() || cookie.isEmpty()) throw RuntimeException("webmailtemp: token 数据无效")
        return username to cookie
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE_URL/api/create", HEADERS)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("webmailtemp: 响应数据无效")
        if (!jsonBool(data, "success")) throw RuntimeException("webmailtemp: 创建邮箱失败")
        val address = jsonStr(data, "email").trim()
        val username = jsonStr(data, "username").trim()
        if (address.isEmpty() || !address.contains("@") || username.isEmpty()) {
            throw RuntimeException("webmailtemp: 响应数据无效")
        }
        val cookie = extractAllCookies(resp.setCookies)
        if (cookie.isEmpty()) throw RuntimeException("webmailtemp: 未获取到 cookie")
        return EmailInfo(email = address, channel = "webmailtemp", token = encodeToken(username, cookie))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("webmailtemp: 邮箱地址为空")
        val (username, cookie) = decodeToken(info.token)
        val headers = HEADERS + ("Cookie" to cookie)
        val resp = ProviderUtil.httpGet("$BASE_URL/api/check/${ProviderUtil.urlEncode(username)}", headers)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val emails = data["emails"] as? JsonArray ?: return emptyList()
        return emails.mapNotNull { it as? JsonObject }.map { item ->
            val flat = mapOf(
                "id" to jsonStr(item, "id"),
                "from" to jsonStr(item, "from"),
                "to" to address,
                "subject" to jsonStr(item, "subject"),
                "body" to ProviderUtil.firstNonEmpty(jsonStr(item, "body"), jsonStr(item, "html")),
                "date" to ProviderUtil.firstNonEmpty(jsonStr(item, "received_at"), jsonStr(item, "timestamp")),
            )
            Normalize.fromMap(flat, address)
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""

/** 从 JSON 对象读取布尔字段（文件私有）。 */
private fun jsonBool(obj: JsonObject, key: String): Boolean =
    (obj[key] as? JsonPrimitive)?.booleanOrNull ?: false

/** 合并所有 Set-Cookie 的 name=value 段为一条 Cookie 头（文件私有）。 */
private fun extractAllCookies(setCookies: List<String>): String {
    val jar = LinkedHashMap<String, String>()
    for (sc in setCookies) {
        val seg = sc.split(";")[0].trim()
        val eq = seg.indexOf('=')
        if (eq > 0) jar[seg.substring(0, eq)] = seg.substring(eq + 1)
    }
    return jar.entries.joinToString("; ") { "${it.key}=${it.value}" }
}
