package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * HarakiriMail 渠道（harakirimail.com）。
 *
 * 无需认证、无需 Cookie、无需 Token 的简单 REST API。
 */
object Harakirimail : Provider {

    private const val BASE = "https://harakirimail.com"
    private val HEADERS = mapOf(
        "Accept" to "application/json, text/plain, */*",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val name = ProviderUtil.randomString(12)
        val email = "$name@harakirimail.com"
        // 调用收件箱接口验证地址可用
        ProviderUtil.httpGet("$BASE/api/v1/inbox/$name", HEADERS).ensureSuccess()
        return EmailInfo(email = email, channel = "harakirimail")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        if (e.isBlank()) throw RuntimeException("harakirimail: 邮箱地址为空")
        val name = if (e.contains("@")) e.substringBefore("@") else e
        val resp = ProviderUtil.httpGet("$BASE/api/v1/inbox/${ProviderUtil.urlEncode(name)}", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val rows = root["emails"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { row ->
            val emailId = jsonStr(row, "_id")
            val detail = fetchBody(emailId)
            val flat = mapOf(
                "id" to emailId,
                "from" to jsonStr(row, "from"),
                "to" to e,
                "subject" to jsonStr(row, "subject"),
                "date" to jsonStr(row, "received"),
                "body" to ProviderUtil.firstNonEmpty(
                    jsonStr(detail, "body_text"), jsonStr(detail, "text"),
                    jsonStr(detail, "body_html"), jsonStr(detail, "html"),
                ),
            )
            Normalize.fromMap(flat, e)
        }
    }

    /** 拉取单封邮件正文详情。 */
    private suspend fun fetchBody(emailId: String): JsonObject {
        if (emailId.isEmpty()) return JsonObject(emptyMap())
        return try {
            val resp = ProviderUtil.httpGet("$BASE/api/v1/email/${ProviderUtil.urlEncode(emailId)}", HEADERS)
            resp.ensureSuccess()
            Http.json.parseToJsonElement(resp.body) as? JsonObject ?: JsonObject(emptyMap())
        } catch (_: Exception) {
            JsonObject(emptyMap())
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
