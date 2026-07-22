package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.util.UUID

/**
 * BestTempMail 渠道（best-temp-mail.com）。
 *
 * POST /api/v3/createEmail 创建（客户端生成 UUID 作为 intToken），POST /api/v3/getEmailList 收信。
 * token 存储 JSON {intToken, id, update_tag}。
 */
object BestTempMail : Provider {

    private const val BASE = "https://best-temp-mail.com"
    private val HEADERS = mapOf(
        "Content-Type" to "application/json",
        "Referer" to "$BASE/",
        "Origin" to BASE,
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val intToken = UUID.randomUUID().toString()
        val payload = buildJsonObject { put("intToken", intToken) }.toString()
        val resp = ProviderUtil.httpPost("$BASE/api/v3/createEmail", payload, "application/json", HEADERS)
        resp.ensureSuccess()
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("best-temp-mail: 创建邮箱失败")
        if (jsonStr(body, "status") != "success") throw RuntimeException("best-temp-mail: 创建邮箱失败")
        val data = body["data"] as? JsonObject ?: throw RuntimeException("best-temp-mail: 响应缺少 data")
        val address = jsonStr(data, "address")
        if (address.isBlank() || !address.contains("@")) {
            throw RuntimeException("best-temp-mail: 返回的邮箱地址无效")
        }
        val token = buildJsonObject {
            put("intToken", intToken)
            put("id", jsonStr(data, "id"))
            put("update_tag", jsonStr(data, "update_tag"))
        }.toString()
        return EmailInfo(email = address, channel = "best-temp-mail", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val tok = Http.json.parseToJsonElement(info.token) as? JsonObject
            ?: throw RuntimeException("best-temp-mail: 无效的 token")
        val reqBody = buildJsonObject {
            put("address", info.email)
            put("id", jsonStr(tok, "id"))
            put("intToken", jsonStr(tok, "intToken"))
            put("update_tag", jsonStr(tok, "update_tag"))
        }.toString()
        val resp = ProviderUtil.httpPost("$BASE/api/v3/getEmailList", reqBody, "application/json", HEADERS)
        resp.ensureSuccess()
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val data = body["data"] as? JsonObject ?: return emptyList()
        if ((data["hasNewEmail"] as? JsonPrimitive)?.booleanOrNull == false) return emptyList()
        val emails = data["emails"] as? JsonArray ?: return emptyList()
        return emails.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, info.email) }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
