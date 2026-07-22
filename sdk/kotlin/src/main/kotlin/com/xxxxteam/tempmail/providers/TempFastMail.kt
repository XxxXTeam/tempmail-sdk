package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * TempFastMail 渠道（tempfastmail.com）。
 *
 * POST /api/email-box 创建，token 为 uuid；GET /api/email-box/{uuid}/emails 收信。
 */
object TempFastMail : Provider {

    private const val BASE_URL = "https://tempfastmail.com"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 创建临时邮箱，token 为 uuid。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$BASE_URL/api/email-box", null, null, HEADERS)
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("tempfastmail: 创建邮箱响应无效")
        val email = jsonStr(data, "email").trim()
        val uuid = jsonStr(data, "uuid").trim()
        if (email.isEmpty() || !email.contains("@") || uuid.isEmpty()) {
            throw RuntimeException("tempfastmail: 创建邮箱响应无效")
        }
        return EmailInfo(email = email, channel = "tempfastmail", token = uuid)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val uuid = info.token.trim()
        val address = info.email.trim()
        if (uuid.isEmpty()) throw RuntimeException("tempfastmail: token 为空")
        if (address.isEmpty()) throw RuntimeException("tempfastmail: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE_URL/api/email-box/${ProviderUtil.urlEncode(uuid)}/emails", HEADERS)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { item ->
            val msgUuid = jsonStr(item, "uuid").trim()
            val src = if (msgUuid.isNotEmpty()) fetchDetail(uuid, msgUuid) ?: item else item
            Normalize.fromJson(buildRaw(src, address), address)
        }
    }

    /** 拉取单封邮件详情。 */
    private suspend fun fetchDetail(boxUuid: String, msgUuid: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(
                "$BASE_URL/api/email-box/${ProviderUtil.urlEncode(boxUuid)}/email/${ProviderUtil.urlEncode(msgUuid)}",
                HEADERS,
            )
            if (!resp.isOk) return null
            Http.json.parseToJsonElement(resp.body) as? JsonObject
        } catch (_: Exception) {
            null
        }
    }

    /** 覆盖 id/to/date 后交由 Normalize 做候选字段提取。 */
    private fun buildRaw(obj: JsonObject, recipient: String): JsonObject = buildJsonObject {
        obj.forEach { (k, v) -> put(k, v) }
        put("id", jsonStr(obj, "uuid"))
        if (obj["to"] == null) put("to", ProviderUtil.firstNonEmpty(jsonStr(obj, "real_to"), recipient))
        if (obj["date"] == null) put("date", jsonStr(obj, "received_at"))
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
