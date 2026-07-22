package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Maildrop 渠道（maildrop.cx）。
 *
 * GET /api/suffixes.php 获取域名，GET /api/emails.php 获取邮件。
 * 无状态：域名内部随机选取。
 */
object Maildrop : Provider {

    private const val BASE = "https://maildrop.cx"
    private const val EXCLUDED = "transformer.edu.kg"
    private val HEADERS = mapOf(
        "Accept" to "application/json, text/plain, */*",
        "Referer" to "https://maildrop.cx/zh-cn/app",
        "User-Agent" to "Mozilla/5.0",
        "X-Requested-With" to "XMLHttpRequest",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$BASE/api/suffixes.php", HEADERS)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray
            ?: throw RuntimeException("maildrop: 域名响应无效")
        val suffixes = arr.mapNotNull { (it as? JsonPrimitive)?.contentOrNull?.trim() }
            .filter { it.isNotEmpty() && !it.equals(EXCLUDED, ignoreCase = true) }
        if (suffixes.isEmpty()) throw RuntimeException("maildrop: 无可用域名")
        val dom = suffixes.random()
        val email = ProviderUtil.randomString(10) + "@" + dom
        return EmailInfo(email = email, channel = "maildrop")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        if (addr.isBlank()) throw RuntimeException("maildrop: 地址为空")
        val url = "$BASE/api/emails.php?addr=${ProviderUtil.urlEncode(addr)}&page=1&limit=20"
        val resp = ProviderUtil.httpGet(url, HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val rows = root["emails"] as? JsonArray ?: return emptyList()
        return rows.mapNotNull { it as? JsonObject }.map { row ->
            val flat = mapOf(
                "id" to jsonStr(row, "id"),
                "from" to jsonStr(row, "from_addr"),
                "to" to addr,
                "subject" to jsonStr(row, "subject"),
                "body" to jsonStr(row, "description"),
                "date" to jsonStr(row, "date"),
            )
            Normalize.fromMap(flat, addr)
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
