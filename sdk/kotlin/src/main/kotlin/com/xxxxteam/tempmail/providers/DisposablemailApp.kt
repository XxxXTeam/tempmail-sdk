package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * disposablemail.app 渠道（disposablemail.app）。
 *
 * POST /api/inbox 创建收件箱，GET /api/inbox/emails?token= 收信。token 存入 EmailInfo.token。
 */
object DisposablemailApp : Provider {

    private const val BASE = "https://disposablemail.app"

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val h = mapOf(
            "Content-Type" to "application/json",
            "Accept" to "application/json",
            "Referer" to "$BASE/",
            "Origin" to BASE,
        )
        val resp = ProviderUtil.httpPost("$BASE/api/inbox", "{}", "application/json", h)
        resp.ensureSuccess()
        val obj = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("disposablemail-app: 响应格式异常")
        val address = jsonStr(obj, "address")
        val token = jsonStr(obj, "token")
        if (address.isBlank() || !address.contains("@")) {
            throw RuntimeException("disposablemail-app: 返回的邮箱地址无效")
        }
        if (token.isBlank()) throw RuntimeException("disposablemail-app: 返回的 token 为空")
        return EmailInfo(email = address, channel = "disposablemail-app", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        val h = mapOf("Accept" to "application/json", "Referer" to "$BASE/")
        val resp = ProviderUtil.httpGet(
            "$BASE/api/inbox/emails?token=${ProviderUtil.urlEncode(info.token)}", h)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val emails = root["emails"] as? JsonArray ?: return emptyList()
        return emails.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, e) }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
