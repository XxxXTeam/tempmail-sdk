package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Neighbours 渠道（neighbours.sh，多域名）。
 *
 * 无状态：generate 拉取域名列表后本地拼接邮箱，getEmails 按需拉取每封详情。
 */
object Neighbours : Provider {

    private const val CHANNEL = "neighbours"
    private const val API_BASE = "https://neighbours.sh/api/v1"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val domains = listDomains()
        val selected = domains.random()
        val email = "sdk" + ProviderUtil.randomString(16) + "@" + selected
        return EmailInfo(email = email, channel = CHANNEL)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isBlank()) throw RuntimeException("neighbours: 邮箱地址为空")
        val data = requestJson("/inbox/${ProviderUtil.urlEncode(address)}", allow404 = true) ?: return emptyList()
        val rows = data["data"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { row ->
            val uid = jsonStr(row, "uid").trim()
            val raw = if (uid.isNotEmpty()) fetchDetail(address, uid) ?: row else row
            Normalize.fromMap(flattenMessage(raw, address), address)
        }
    }

    /** 拉取可用域名列表。 */
    private suspend fun listDomains(): List<String> {
        val data = requestJson("/config/domains", allow404 = false)
        val nested = data?.get("data") as? JsonObject
        val arr = nested?.get("domains") as? JsonArray
        val domains = arr.orEmpty().mapNotNull { (it as? JsonPrimitive)?.contentOrNull?.trim()?.lowercase() }
            .filter { it.isNotEmpty() }
        if (domains.isEmpty()) throw RuntimeException("neighbours: 域名列表为空")
        return domains
    }

    /** 发起 JSON 请求，allow404 时 404 返回 null。 */
    private suspend fun requestJson(path: String, allow404: Boolean): JsonObject? {
        val resp = ProviderUtil.httpGet(API_BASE + path, HEADERS)
        if (allow404 && resp.statusCode == 404) return null
        resp.ensureSuccess()
        return Http.json.parseToJsonElement(resp.body) as? JsonObject
    }

    /** 拉取单封邮件详情。 */
    private suspend fun fetchDetail(address: String, uid: String): JsonObject? {
        val data = requestJson(
            "/inbox/${ProviderUtil.urlEncode(address)}/${ProviderUtil.urlEncode(uid)}", allow404 = true,
        ) ?: return null
        return data["data"] as? JsonObject
    }

    /** 递归提取首个地址字符串。 */
    private fun firstAddress(value: JsonElement?): String {
        when (value) {
            null, is JsonNull -> return ""
            is JsonPrimitive -> return value.contentOrNull?.trim() ?: ""
            is JsonArray -> {
                for (item in value) {
                    val hit = firstAddress(item)
                    if (hit.isNotEmpty()) return hit
                }
                return ""
            }
            is JsonObject -> {
                val addr = jsonStr(value, "address").trim()
                if (addr.isNotEmpty()) return addr
                val text = jsonStr(value, "text").trim()
                if (text.contains("@")) return text
                return firstAddress(value["value"])
            }
        }
    }

    /** 扁平化单封邮件。 */
    private fun flattenMessage(raw: JsonObject, recipient: String): Map<String, Any?> {
        val to = firstAddress(raw["to"])
        return mapOf(
            "id" to jsonStr(raw, "uid"),
            "from" to firstAddress(raw["from"]),
            "to" to to.ifEmpty { recipient },
            "subject" to jsonStr(raw, "subject"),
            "body" to ProviderUtil.firstNonEmpty(
                jsonStr(raw, "text"), jsonStr(raw, "snippet"), jsonStr(raw, "html"),
            ),
            "date" to ProviderUtil.firstNonEmpty(jsonStr(raw, "date"), jsonStr(raw, "received_at")),
        )
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
