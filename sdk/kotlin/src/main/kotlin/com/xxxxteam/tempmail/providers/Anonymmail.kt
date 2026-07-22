package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Anonymmail 渠道（anonymmail.net）。
 *
 * POST /api/getDomains 取域名，POST /api/create 建箱，POST /api/get 收信（嵌套响应）。
 */
object Anonymmail : Provider {

    private const val BASE = "https://anonymmail.net"
    private val HEADERS = mapOf(
        "Accept" to "*/*",
        "Content-Type" to "application/x-www-form-urlencoded",
        "Referer" to "https://anonymmail.net/",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val domains = fetchDomains()
        if (domains.isEmpty()) throw RuntimeException("anonymmail: no domains available")
        val domain = domains.random()
        val email = "${ProviderUtil.randomString(10)}@$domain"
        val resp = ProviderUtil.httpPost(
            "$BASE/api/create", "email=${ProviderUtil.urlEncode(email)}",
            "application/x-www-form-urlencoded", HEADERS)
        resp.ensureSuccess()
        val obj = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("anonymmail: create inbox failed")
        if ((obj["success"] as? JsonPrimitive)?.booleanOrNull != true) {
            throw RuntimeException("anonymmail: create inbox failed")
        }
        val addr = jsonStr(obj, "email").trim()
        if (addr.isBlank()) throw RuntimeException("anonymmail: missing email in response")
        return EmailInfo(email = addr, channel = "anonymmail")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        if (e.isBlank()) throw RuntimeException("anonymmail: empty email")
        val resp = ProviderUtil.httpPost(
            "$BASE/api/get", "email=${ProviderUtil.urlEncode(e)}",
            "application/x-www-form-urlencoded", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        // 响应格式: {"email@domain": {"created_at": "...", "emails": [...]}}
        val inbox = root[e] as? JsonObject ?: return emptyList()
        val rows = inbox["emails"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { row ->
            val flat = mutableMapOf<String, Any?>()
            row.forEach { (k, v) -> flat[k] = (v as? JsonPrimitive)?.contentOrNull ?: v.toString() }
            // token 映射为 id，body 映射为 html
            if (flat.containsKey("token") && !flat.containsKey("id")) flat["id"] = flat.remove("token")
            if (flat.containsKey("body") && !flat.containsKey("html")) {
                flat["html"] = flat["body"]; flat.remove("body")
            }
            Normalize.fromMap(flat, e)
        }
    }

    /** 拉取可用域名列表。 */
    private suspend fun fetchDomains(): List<String> {
        val resp = ProviderUtil.httpPost(
            "$BASE/api/getDomains", null, "application/x-www-form-urlencoded", HEADERS)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }
            .mapNotNull { jsonStr(it, "domain").trim().ifEmpty { null } }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
