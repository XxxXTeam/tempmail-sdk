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

    /**
     * 通过详情接口获取单封邮件完整内容。
     * GET /api/email_content.php?id={id}
     * 详情响应字段：content(完整HTML) / subject / from_addr / date / attachment(JSON数组字符串)
     *
     * @param id 邮件 ID
     * @return 详情 JsonObject，失败返回 null
     */
    private suspend fun fetchDetail(id: String): JsonObject? {
        if (id.isBlank()) return null
        return try {
            val resp = ProviderUtil.httpGet(
                "$BASE/api/email_content.php?id=${ProviderUtil.urlEncode(id)}", HEADERS)
            if (resp.statusCode < 200 || resp.statusCode >= 300) null
            else Http.json.parseToJsonElement(resp.body) as? JsonObject
        } catch (_: Exception) {
            null
        }
    }

    /**
     * 解析详情接口的 attachment 字段（JSON 字符串）为附件列表。
     */
    private fun parseAttachments(raw: String): List<Map<String, Any>> {
        if (raw.isBlank()) return emptyList()
        return try {
            val arr = Http.json.parseToJsonElement(raw) as? JsonArray ?: return emptyList()
            arr.mapNotNull { it as? JsonObject }.mapNotNull { obj ->
                val filename = (obj["filename"] as? JsonPrimitive)?.contentOrNull?.trim().orEmpty()
                if (filename.isEmpty()) return@mapNotNull null
                val entry = mutableMapOf<String, Any>("filename" to filename)
                (obj["size"] as? JsonPrimitive)?.longOrNull?.let { entry["size"] = it }
                entry
            }
        } catch (_: Exception) {
            emptyList()
        }
    }

    /**
     * 获取邮件列表并对每封邮件补拉详情。
     * 1. GET /api/emails.php 拉取列表（仅含 description 摘要）
     * 2. 对每封邮件 GET /api/email_content.php?id={id} 拉取详情（含 content 完整 HTML）
     * 3. 详情失败时保留列表 description 作为回退
     */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        if (addr.isBlank()) throw RuntimeException("maildrop: 地址为空")
        val url = "$BASE/api/emails.php?addr=${ProviderUtil.urlEncode(addr)}&page=1&limit=20"
        val resp = ProviderUtil.httpGet(url, HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val rows = root["emails"] as? JsonArray ?: return emptyList()

        val out = mutableListOf<Email>()
        for (row in rows.mapNotNull { it as? JsonObject }) {
            val id = jsonStr(row, "id")
            val desc = jsonStr(row, "description")
            var from = jsonStr(row, "from_addr")
            var subject = jsonStr(row, "subject")
            var date = jsonStr(row, "date")
            var html = ""
            var attachments: List<Map<String, Any>> = emptyList()

            /* 拉取详情覆盖 html/from/subject/date/attachments */
            if (id.isNotBlank()) {
                val detail = fetchDetail(id)
                if (detail != null) {
                    val dContent = jsonStr(detail, "content")
                    if (dContent.isNotBlank()) html = dContent
                    val dFrom = jsonStr(detail, "from_addr")
                    if (dFrom.isNotBlank()) from = dFrom
                    val dSubj = jsonStr(detail, "subject")
                    if (dSubj.isNotBlank()) subject = dSubj
                    val dDate = jsonStr(detail, "date")
                    if (dDate.isNotBlank()) date = dDate
                    attachments = parseAttachments(jsonStr(detail, "attachment"))
                }
            }

            val flat = mapOf(
                "id" to id,
                "from" to from,
                "to" to addr,
                "subject" to subject,
                "text" to desc,
                "html" to html,
                "date" to date,
                "attachments" to attachments,
            )
            out.add(Normalize.fromMap(flat, addr))
        }
        return out
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
