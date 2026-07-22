package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * neighbours.sh 渠道实现（固定域名 neighbours.sh）。
 *
 * 公共收件箱模式，无需注册，本地生成随机地址即可收信。
 */
object NeighboursSh : Provider {

    private const val CHANNEL = "neighbours-sh"
    private const val API_BASE = "https://neighbours.sh/api/v1"
    private const val DOMAIN = "neighbours.sh"

    private val headers = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    )

    /** 创建 neighbours.sh 临时邮箱（本地生成随机地址）。 */
    override suspend fun generate(): EmailInfo {
        val email = "sdk${ProviderUtil.randomString(10)}@$DOMAIN"
        return EmailInfo(email = email, channel = CHANNEL, token = email)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("neighbours-sh: 缺少邮箱地址")
        val data = requestJson("/inbox/${ProviderUtil.urlEncode(address)}") ?: return emptyList()
        val rows = ProviderUtil.arr(data, "data") ?: return emptyList()
        val result = mutableListOf<Email>()
        for (item in rows.filterIsInstance<JsonObject>()) {
            val uid = ProviderUtil.str(item, "uid").trim()
            if (uid.isEmpty()) continue
            val detail = fetchDetail(address, uid) ?: continue
            result.add(Normalize.fromMap(flattenMessage(detail, address), address))
        }
        return result
    }

    /** 请求 JSON，404 返回 null。 */
    private suspend fun requestJson(path: String): JsonObject? {
        val resp = ProviderUtil.httpGet(API_BASE + path, headers)
        if (resp.statusCode == 404) return null
        resp.ensureSuccess()
        return ProviderUtil.parseObject(resp.body)
    }

    private suspend fun fetchDetail(address: String, uid: String): JsonObject? {
        val data = requestJson("/inbox/${ProviderUtil.urlEncode(address)}/${ProviderUtil.urlEncode(uid)}") ?: return null
        return data["data"] as? JsonObject
    }

    /** 从 from/to 节点递归提取首个地址。 */
    private fun firstAddress(value: JsonElement?): String {
        when (value) {
            null, is JsonNull -> return ""
            is JsonPrimitive -> return value.content.trim()
            is JsonArray -> {
                for (item in value) {
                    val hit = firstAddress(item)
                    if (hit.isNotEmpty()) return hit
                }
                return ""
            }
            is JsonObject -> {
                val addr = ProviderUtil.str(value, "address").trim()
                if (addr.isNotEmpty()) return addr
                val text = ProviderUtil.str(value, "text").trim()
                if (text.contains("@")) return text
                return firstAddress(value["value"])
            }
        }
    }

    private fun flattenMessage(detail: JsonObject, recipient: String): Map<String, Any?> {
        val to = firstAddress(detail["to"])
        return mapOf(
            "id" to ProviderUtil.str(detail, "uid"),
            "from" to firstAddress(detail["from"]),
            "to" to to.ifEmpty { recipient },
            "subject" to ProviderUtil.str(detail, "subject"),
            "text" to ProviderUtil.str(detail, "text"),
            "html" to ProviderUtil.str(detail, "html"),
            "date" to ProviderUtil.str(detail, "date"),
        )
    }
}
