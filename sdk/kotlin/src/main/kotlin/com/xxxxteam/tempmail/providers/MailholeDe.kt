package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * MailholeDe 渠道实现 — https://mailhole.de
 *
 * 公共临时邮箱，无需认证，token 即邮箱地址本身。
 */
object MailholeDe : Provider {

    private const val BASE_URL = "https://mailhole.de"
    private val EMAIL_RE = Regex("([a-z0-9.]+@mailhole\\.de)")

    /** 创建 mailhole.de 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE_URL/api/random", mapOf("Accept" to "text/html"))
        resp.ensureSuccess()
        val email = EMAIL_RE.find(resp.body)?.groupValues?.get(1)
            ?: throw RuntimeException("mailhole-de: 无法从响应中解析邮箱地址")
        return EmailInfo(email = email, channel = "mailhole-de", token = email)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.token.ifBlank { info.email }.trim()
        if (addr.isEmpty()) throw RuntimeException("mailhole-de: 缺少邮箱地址")
        val resp = ProviderUtil.httpGet(
            "$BASE_URL/json/" + ProviderUtil.urlEncode(addr), mapOf("Accept" to "application/json"))
        resp.ensureSuccess()
        val body = resp.body.trim()
        if (body.isEmpty() || body == "[]") return emptyList()
        val arr = Http.json.parseToJsonElement(body) as? JsonArray ?: return emptyList()
        val result = ArrayList<Email>(arr.size)
        for (item in arr) {
            val msg = item as? JsonObject ?: continue
            val row = mapOf(
                "id" to str(msg, "id"),
                "from" to first(msg, "sender", "from"),
                "to" to addr,
                "subject" to str(msg, "subject"),
                "text" to first(msg, "body", "text"),
                "html" to first(msg, "html", "body"),
                "received_at" to first(msg, "date", "received"),
            )
            result.add(Normalize.fromMap(row, addr))
        }
        return result
    }

    private fun str(obj: JsonObject, key: String): String =
        (obj[key] as? JsonPrimitive)?.content ?: ""

    private fun first(obj: JsonObject, vararg keys: String): String {
        for (k in keys) {
            val v = str(obj, k)
            if (v.isNotEmpty()) return v
        }
        return ""
    }
}
