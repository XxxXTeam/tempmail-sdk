package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * TempMail Plus 渠道（tempmail.plus）。
 *
 * 无需认证、无需 Cookie、无需 Token 的简单 REST API。域名别名共享同一实现，
 * 因此参数化为 (channel, domain)，域名为空时使用默认 mailto.plus。
 *
 * @property channel 渠道标识
 * @property domain 域名，空串则使用默认域名
 */
class TempMailPlus(private val channel: String, private val domain: String) : Provider {

    private companion object {
        const val API_BASE = "https://tempmail.plus/api/mails"
        const val DEFAULT_DOMAIN = "mailto.plus"
        val HEADERS = mapOf(
            "Accept" to "application/json",
            "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
                "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
            "Referer" to "https://tempmail.plus/",
            "Origin" to "https://tempmail.plus",
        )
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val selectedDomain = domain.trim().ifEmpty { DEFAULT_DOMAIN }
        val email = ProviderUtil.randomString(12) + "@" + selectedDomain
        // 调用列表接口验证地址可用
        ProviderUtil.httpGet("$API_BASE/?email=${ProviderUtil.urlEncode(email)}&epin=", HEADERS).ensureSuccess()
        return EmailInfo(email = email, channel = channel)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        if (e.isBlank()) throw RuntimeException("tempmail-plus: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$API_BASE/?email=${ProviderUtil.urlEncode(e)}&epin=", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val rows = root["mail_list"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { raw ->
            val mailId = jsonStr(raw, "mail_id")
            val detail = fetchBody(mailId, e)
            val flat = mapOf(
                "id" to mailId,
                "from" to ProviderUtil.firstNonEmpty(jsonStr(raw, "from_mail"), jsonStr(raw, "from_name")),
                "to" to e,
                "subject" to jsonStr(raw, "subject"),
                "date" to jsonStr(raw, "time"),
                "body" to ProviderUtil.firstNonEmpty(detail.first, detail.second),
            )
            Normalize.fromMap(flat, e)
        }
    }

    /** 拉取单封邮件正文，返回 (html, text)。 */
    private suspend fun fetchBody(mailId: String, email: String): Pair<String, String> {
        if (mailId.isEmpty()) return "" to ""
        return try {
            val resp = ProviderUtil.httpGet(
                "$API_BASE/${ProviderUtil.urlEncode(mailId)}?email=${ProviderUtil.urlEncode(email)}&epin=",
                HEADERS,
            )
            if (!resp.isOk) return "" to ""
            val obj = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return "" to ""
            jsonStr(obj, "html") to jsonStr(obj, "text")
        } catch (_: Exception) {
            "" to ""
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
