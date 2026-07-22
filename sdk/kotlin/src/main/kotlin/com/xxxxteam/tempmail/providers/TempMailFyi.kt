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
 * temp-mail.fyi 渠道（temp-mail.fyi）。
 *
 * GET / 取 PHPSESSID + CSRF，POST /api/generate_email.php 创建，
 * POST /api/get_emails.php 收信。token 使用 base64 编码，前缀 "tmf1:"。
 */
object TempMailFyi : Provider {

    private const val BASE_URL = "https://temp-mail.fyi"
    private const val TOK_PREFIX = "tmf1:"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

    private fun apiHeaders(csrf: String, cookie: String) = mapOf(
        "User-Agent" to UA,
        "Accept" to "application/json, text/javascript, */*; q=0.01",
        "Accept-Language" to "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        "Content-Type" to "application/json",
        "X-CSRF-Token" to csrf,
        "X-Requested-With" to "XMLHttpRequest",
        "Referer" to "$BASE_URL/",
        "Cookie" to cookie,
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE_URL/", mapOf(
            "User-Agent" to UA,
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"))
        resp.ensureSuccess()
        val cookie = extractAllCookies(resp.setCookies)
        val csrf = Regex("csrfToken\"\\s*value=\"([^\"]+)\"").find(resp.body)?.groupValues?.get(1) ?: ""
        if (csrf.isBlank()) throw RuntimeException("tempmail-fyi: 未能从首页提取 CSRF token")
        val resp2 = ProviderUtil.httpPost("$BASE_URL/api/generate_email.php", "{}",
            "application/json", apiHeaders(csrf, cookie))
        resp2.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp2.body) as? JsonObject
            ?: throw RuntimeException("tempmail-fyi: 创建邮箱失败")
        if ((data["success"] as? JsonPrimitive)?.booleanOrNull != true) {
            throw RuntimeException("tempmail-fyi: 创建邮箱失败")
        }
        val address = jsonStr(data, "email_address").trim()
        if (address.isBlank() || !address.contains("@")) throw RuntimeException("tempmail-fyi: 邮箱地址无效")
        return EmailInfo(email = address, channel = "tempmail-fyi", token = encodeToken(csrf, cookie))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isBlank()) throw RuntimeException("tempmail-fyi: 邮箱地址为空")
        val (csrf, cookie) = decodeToken(info.token)
        val body = buildJsonObject { put("email_address", address) }.toString()
        val resp = ProviderUtil.httpPost("$BASE_URL/api/get_emails.php", body,
            "application/json", apiHeaders(csrf, cookie))
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        if ((data["success"] as? JsonPrimitive)?.booleanOrNull != true) return emptyList()
        val emails = data["emails"] as? JsonArray ?: return emptyList()
        return emails.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, address) }
    }

    /** 编码 token（base64，前缀 tmf1:）。 */
    private fun encodeToken(csrf: String, cookie: String): String {
        val raw = buildJsonObject { put("t", csrf); put("c", cookie) }.toString()
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.toByteArray(StandardCharsets.UTF_8))
    }

    /** 解码 token，返回 [csrf, cookie]。 */
    private fun decodeToken(token: String): Pair<String, String> {
        if (!token.startsWith(TOK_PREFIX)) throw RuntimeException("tempmail-fyi: 无效的 token")
        val decoded = Base64.getDecoder().decode(token.substring(TOK_PREFIX.length))
        val obj = Http.json.parseToJsonElement(String(decoded, StandardCharsets.UTF_8)) as? JsonObject
            ?: throw RuntimeException("tempmail-fyi: 无效的 token")
        val csrf = jsonStr(obj, "t").trim()
        val cookie = jsonStr(obj, "c").trim()
        if (csrf.isEmpty()) throw RuntimeException("tempmail-fyi: token 中缺少 CSRF")
        return csrf to cookie
    }

    /** 提取所有 Set-Cookie 的 name=value 串。 */
    private fun extractAllCookies(setCookies: List<String>): String =
        setCookies.map { it.split(";")[0].trim() }.filter { it.contains("=") }.joinToString("; ")
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
