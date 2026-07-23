package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Mail Sunls 渠道（mail.sunls.de）。
 *
 * 无 token 无 session。从 /api/domain 获取域名列表，本地随机生成地址。
 */
object MailSunls : Provider {

    private const val BASE = "https://mail.sunls.de"
    private val HEADERS = mapOf(
        "Accept" to "application/json, text/plain, */*",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer" to "https://mail.sunls.de/",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val domains = fetchDomains()
        val domain = domains.random()
        val local = ProviderUtil.randomString(10)
        return EmailInfo(email = "$local@$domain", channel = "mail-sunls")
    }

    /**
     * 通过详情接口获取单封邮件完整正文。
     * GET /api/fetch/{id}
     *
     * @param id 邮件 ID
     * @return 详情 JsonObject（含 text/html），失败返回 null
     */
    private suspend fun fetchDetail(id: String): JsonObject? {
        if (id.isBlank()) return null
        return try {
            val resp = ProviderUtil.httpGet(
                "$BASE/api/fetch/${ProviderUtil.urlEncode(id)}", HEADERS)
            if (resp.statusCode < 200 || resp.statusCode >= 300) null
            else Http.json.parseToJsonElement(resp.body) as? JsonObject
        } catch (_: Exception) {
            null
        }
    }

    /** 从列表条目提取邮件 ID（支持多字段）。 */
    private fun extractId(row: JsonObject): String {
        for (key in arrayOf("id", "_id", "mail_id", "messageId", "message_id")) {
            (row[key] as? JsonPrimitive)?.contentOrNull?.trim()?.let {
                if (it.isNotEmpty()) return it
            }
        }
        return ""
    }

    /**
     * 获取邮件列表。
     * 1. GET /api/fetch?to={email} 拉取列表元数据
     * 2. 对每封邮件 GET /api/fetch/{id} 拉取详情（含完整 text/html）
     * 3. 详情失败时保留列表字段作为回退
     */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        if (addr.isBlank()) throw RuntimeException("mail-sunls: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE/api/fetch?to=${ProviderUtil.urlEncode(addr)}", HEADERS)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()

        val out = mutableListOf<Email>()
        for (row in arr.mapNotNull { it as? JsonObject }) {
            val id = extractId(row)
            /* 合并列表 + 详情，详情字段优先覆盖 */
            val merged = row.toMutableMap()
            if (id.isNotEmpty()) {
                fetchDetail(id)?.forEach { (k, v) -> merged[k] = v }
            }
            out.add(Normalize.fromJson(JsonObject(merged), addr))
        }
        return out
    }

    /** 拉取可用域名列表。 */
    private suspend fun fetchDomains(): List<String> {
        val resp = ProviderUtil.httpGet("$BASE/api/domain", HEADERS)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray
            ?: throw RuntimeException("mail-sunls: 域名列表响应格式无效")
        val out = arr.mapNotNull { (it as? JsonPrimitive)?.contentOrNull?.trim() }
            .filter { it.isNotEmpty() }
        if (out.isEmpty()) throw RuntimeException("mail-sunls: 无可用域名")
        return out
    }
}
