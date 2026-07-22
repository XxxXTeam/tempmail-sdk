package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * awamail.com 渠道。
 *
 * POST /welcome/change_mailbox 创建邮箱并提取 awamail_session cookie 作为 token，
 * GET /welcome/get_emails 获取邮件列表。
 */
object Awamail : Provider {

    private const val BASE_URL = "https://awamail.com/welcome"

    private fun defaultHeaders(): MutableMap<String, String> = linkedMapOf(
        "User-Agent" to "Mozilla/5.0",
        "Accept" to "application/json, text/javascript, */*; q=0.01",
        "Origin" to "https://awamail.com",
        "Referer" to "https://awamail.com/?lang=zh",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val h = defaultHeaders().apply { put("Content-Length", "0") }
        val resp = ProviderUtil.httpPost("$BASE_URL/change_mailbox", "", "application/json", h)
        resp.ensureSuccess()

        var sessionCookie = ""
        for (c in resp.setCookies) {
            val idx = c.indexOf("awamail_session=")
            if (idx >= 0) {
                val seg = c.substring(idx)
                val semi = seg.indexOf(';')
                sessionCookie = if (semi < 0) seg else seg.substring(0, semi)
                break
            }
        }
        if (sessionCookie.isEmpty()) throw RuntimeException("awamail: 未获取到 session cookie")

        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject
        if (body == null || !jsonBool(body, "success")) throw RuntimeException("awamail: 创建邮箱失败")
        val data = body["data"] as? JsonObject ?: throw RuntimeException("awamail: 响应缺少 data")
        val email = jsonStr(data, "email_address")
        return EmailInfo(email = email, channel = "awamail", token = sessionCookie)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val h = defaultHeaders().apply {
            put("Cookie", info.token)
            put("X-Requested-With", "XMLHttpRequest")
        }
        val resp = ProviderUtil.httpGet("$BASE_URL/get_emails", h)
        resp.ensureSuccess()
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject
        if (body == null || !jsonBool(body, "success")) throw RuntimeException("awamail: 获取邮件失败")
        val data = body["data"] as? JsonObject ?: return emptyList()
        val emails = data["emails"] as? JsonArray ?: return emptyList()
        return emails.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, info.email) }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""

/** 从 JSON 对象读取布尔字段（文件私有）。 */
private fun jsonBool(obj: JsonObject, key: String): Boolean =
    (obj[key] as? JsonPrimitive)?.booleanOrNull ?: false
