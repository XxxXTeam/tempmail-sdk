package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * FakeEmailSite 渠道（fake-email.site）。
 *
 * POST /api/temporary-address 创建，GET /api/inbox/poll 收信。
 */
object FakeEmailSite : Provider {

    private const val BASE = "https://api.fake-email.site"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "Content-Type" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$BASE/api/temporary-address", "{}", "application/json", HEADERS)
        resp.ensureSuccess()
        val obj = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("fake-email-site: 无效响应格式")
        val email = jsonStr(obj, "temp_email_addr").trim()
        if (email.isBlank()) throw RuntimeException("fake-email-site: 响应中未找到临时邮箱地址")
        return EmailInfo(email = email, channel = "fake-email-site")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        if (addr.isBlank()) throw RuntimeException("fake-email-site: empty email")
        val getHeaders = HEADERS - "Content-Type"
        val resp = ProviderUtil.httpGet("$BASE/api/inbox/poll?address=${ProviderUtil.urlEncode(addr)}", getHeaders)
        if (resp.statusCode == 404) return emptyList()
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val msgs = root["messages"] as? JsonArray ?: return emptyList()
        return msgs.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, addr) }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
