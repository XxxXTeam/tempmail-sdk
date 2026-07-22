package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Temporam 渠道（temporam.com）。
 *
 * 无状态：generate 拉取域名后本地拼接邮箱，getEmails 按需拉取每封详情。
 */
object Temporam : Provider {

    private const val ORIGIN = "https://temporam.com"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "Accept-Encoding" to "gzip, deflate, br",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val domains = fetchDomains()
        val selected = domains.random()
        val email = "sdk" + ProviderUtil.randomString(18) + "@" + selected
        return EmailInfo(email = email, channel = "temporam")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val recipient = info.email.trim()
        if (recipient.isBlank()) throw RuntimeException("temporam: 缺少邮箱地址")
        val data = getJson("/api/emails?email=${ProviderUtil.urlEncode(recipient)}&limit=50")
        val rows = data["data"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { row ->
            val msgId = ProviderUtil.firstNonEmpty(jsonStr(row, "id"), jsonStr(row, "uuid"))
            var raw = row
            if (msgId.isNotEmpty()) {
                try {
                    val detail = getJson("/api/emails/${ProviderUtil.urlEncode(msgId)}")
                    (detail["data"] as? JsonObject)?.let { raw = it }
                } catch (_: Exception) {
                    // 回退到列表摘要
                }
            }
            Normalize.fromMap(flatten(raw, recipient), recipient)
        }
    }

    /** 拉取可用域名列表。 */
    private suspend fun fetchDomains(): List<String> {
        val data = getJson("/api/domains")
        val arr = data["data"] as? JsonArray
        val domains = arr.orEmpty().mapNotNull { it as? JsonObject }
            .map { jsonStr(it, "domain").trim() }
            .filter { it.isNotEmpty() }
        if (domains.isEmpty()) throw RuntimeException("temporam: 域名列表为空")
        return domains
    }

    /** 发起 JSON 请求。 */
    private suspend fun getJson(path: String): JsonObject {
        val resp = ProviderUtil.httpGet(ORIGIN + path, HEADERS)
        resp.ensureSuccess()
        return Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("temporam: 无效的 JSON 响应")
    }

    /** 扁平化单封邮件。 */
    private fun flatten(raw: JsonObject, email: String): Map<String, Any?> {
        val to = ProviderUtil.firstNonEmpty(jsonStr(raw, "toEmail"), jsonStr(raw, "to"))
        return mapOf(
            "id" to ProviderUtil.firstNonEmpty(jsonStr(raw, "id"), jsonStr(raw, "uuid")),
            "from" to ProviderUtil.firstNonEmpty(jsonStr(raw, "fromEmail"), jsonStr(raw, "from")),
            "to" to to.ifEmpty { email },
            "subject" to jsonStr(raw, "subject"),
            "body" to ProviderUtil.firstNonEmpty(jsonStr(raw, "text"), jsonStr(raw, "html")),
            "date" to ProviderUtil.firstNonEmpty(
                jsonStr(raw, "createdAt"), jsonStr(raw, "created_at"), jsonStr(raw, "date"),
            ),
        )
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
