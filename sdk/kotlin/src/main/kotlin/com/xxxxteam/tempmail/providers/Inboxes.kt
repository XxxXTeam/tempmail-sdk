package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Inboxes 渠道（inboxes.com）。
 *
 * 无状态：generate 拉取可用域名后本地拼接邮箱，getEmails 按需拉取每封详情。
 */
object Inboxes : Provider {

    private const val API_BASE = "https://inboxes.com/api/v2"
    private const val DEFAULT_DOMAIN = "blondmail.com"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "Origin" to "https://inboxes.com",
        "Referer" to "https://inboxes.com/",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 获取可用域名列表。 */
    private suspend fun getDomains(): List<String> {
        val resp = ProviderUtil.httpGet("$API_BASE/domain", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("inboxes: 无可用域名")
        val arr = root["domains"] as? JsonArray ?: throw RuntimeException("inboxes: 无可用域名")
        val domains = arr.mapNotNull { it as? JsonObject }
            .map { jsonStr(it, "qdn").trim() }
            .filter { it.isNotEmpty() }
        if (domains.isEmpty()) throw RuntimeException("inboxes: 无可用域名")
        return domains
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val domains = getDomains()
        val selected = if (domains.contains(DEFAULT_DOMAIN)) DEFAULT_DOMAIN else domains[0]
        return EmailInfo(email = ProviderUtil.randomLocalSdk() + "@" + selected, channel = "inboxes")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isBlank()) throw RuntimeException("inboxes: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$API_BASE/inbox/${ProviderUtil.urlEncode(address)}", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val rows = root["msgs"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { row ->
            val uid = jsonStr(row, "uid").trim()
            val src = if (uid.isNotEmpty()) fetchDetail(uid) ?: row else row
            val flat = mapOf(
                "id" to jsonStr(src, "uid"),
                "from" to ProviderUtil.firstNonEmpty(jsonStr(src, "sf"), jsonStr(src, "f")),
                "to" to ProviderUtil.firstNonEmpty(jsonStr(src, "ib"), address),
                "subject" to jsonStr(src, "s"),
                "body" to ProviderUtil.firstNonEmpty(jsonStr(src, "text"), jsonStr(src, "html")),
                "date" to jsonStr(src, "cr"),
            )
            Normalize.fromMap(flat, address)
        }
    }

    /** 拉取单封邮件详情。 */
    private suspend fun fetchDetail(uid: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet("$API_BASE/message/${ProviderUtil.urlEncode(uid)}", HEADERS)
            if (!resp.isOk) return null
            Http.json.parseToJsonElement(resp.body) as? JsonObject
        } catch (_: Exception) {
            null
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
