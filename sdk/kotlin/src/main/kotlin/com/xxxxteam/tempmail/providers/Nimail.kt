package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * nimail 渠道实现 — https://www.nimail.cn
 *
 * 简单 POST 表单 API 临时邮箱，无需认证，token 即邮箱地址本身。
 */
object Nimail : Provider {

    private const val BASE_URL = "https://www.nimail.cn"

    private val HEADERS = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        "Content-Type" to "application/x-www-form-urlencoded",
        "Origin" to BASE_URL,
        "Referer" to "$BASE_URL/",
    )

    /** 创建 nimail.cn 临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val email = ProviderUtil.randomString(10) + "@nimail.cn"
        val body = "mail=" + ProviderUtil.urlEncode(email)
        val resp = ProviderUtil.httpPost(
            "$BASE_URL/api/applymail", body, "application/x-www-form-urlencoded", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("nimail: 创建邮箱失败")
        val user = str(root, "user")
        if (str(root, "success") != "true" || user.isEmpty()) {
            throw RuntimeException("nimail: 创建邮箱失败")
        }
        return EmailInfo(email = user, channel = "nimail", token = user)
    }
    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.token.ifBlank { info.email }.trim()
        if (addr.isEmpty()) throw RuntimeException("nimail: 缺少邮箱地址")
        val body = "mail=" + ProviderUtil.urlEncode(addr) + "&time=0"
        val resp = ProviderUtil.httpPost(
            "$BASE_URL/api/getmails", body, "application/x-www-form-urlencoded", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        if (str(root, "success") != "true") return emptyList()
        val mail = root["mail"] as? JsonArray ?: return emptyList()
        val result = ArrayList<Email>(mail.size)
        for (item in mail) {
            val msg = item as? JsonObject ?: continue
            val row = mapOf(
                "id" to first(msg, "id", "time"),
                "from" to first(msg, "from", "sender"),
                "to" to addr,
                "subject" to first(msg, "subject", "title"),
                "text" to first(msg, "text", "content"),
                "html" to first(msg, "html", "content"),
                "date" to first(msg, "date", "time"),
            )
            result.add(Normalize.fromMap(row, addr))
        }
        return result
    }

    /** 从 JsonObject 读取字符串值（兼容布尔/数字原始值）。 */
    private fun str(obj: JsonObject, key: String): String {
        val v = obj[key] as? JsonPrimitive ?: return ""
        return v.content
    }

    /** 依次尝试多个键，返回首个非空字符串值。 */
    private fun first(obj: JsonObject, vararg keys: String): String {
        for (k in keys) {
            val v = str(obj, k)
            if (v.isNotEmpty()) return v
        }
        return ""
    }
}
