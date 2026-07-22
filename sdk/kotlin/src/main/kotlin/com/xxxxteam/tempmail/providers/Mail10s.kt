package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Mail10s 渠道（mail10s.com）。
 *
 * 无状态：本地随机生成邮箱地址，getEmails 直接以邮箱地址查询收件箱。
 */
object Mail10s : Provider {

    private const val BASE_URL = "https://mail10s.com"
    private const val DOMAIN = "mail10s.com"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 创建临时邮箱（本地随机生成）。 */
    override suspend fun generate(): EmailInfo {
        val email = ProviderUtil.randomLocalSdk() + "@" + DOMAIN
        return EmailInfo(email = email, channel = "mail10s")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isBlank()) throw RuntimeException("mail10s: 邮箱地址为空")
        val resp = ProviderUtil.httpGet(
            "$BASE_URL/api/emails/${ProviderUtil.urlEncode(address)}/inbox", HEADERS,
        )
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val messages = (root["data"] as? JsonObject)?.get("messages") as? JsonArray ?: return emptyList()
        return messages.mapNotNull { it as? JsonObject }.map { obj ->
            val flat = mapOf(
                "id" to jsonStr(obj, "id"),
                "from" to jsonStr(obj, "sender"),
                "to" to address,
                "subject" to jsonStr(obj, "subject"),
                "body" to ProviderUtil.firstNonEmpty(jsonStr(obj, "body_text"), jsonStr(obj, "body_html")),
                "date" to jsonStr(obj, "received_at"),
            )
            Normalize.fromMap(flat, address)
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有，避免跨批次重复声明冲突）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
