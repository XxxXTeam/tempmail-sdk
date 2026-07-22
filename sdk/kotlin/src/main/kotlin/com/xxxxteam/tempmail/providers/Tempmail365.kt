package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.time.Instant
import java.time.format.DateTimeFormatter

/**
 * Tempmail365 渠道（tempmail365.cn）。
 *
 * GET ?action=get_config 获取域名，?action=create_email 创建，?action=fetch_mail 收信。
 */
object Tempmail365 : Provider {

    private const val BASE = "https://tempmail365.cn/tempemail.php"
    private val FALLBACK_DOMAINS = listOf("fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn")
    private val HEADERS = mapOf(
        "Accept" to "application/json, text/plain, */*",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer" to "https://tempmail365.cn/",
        "sec-ch-ua" to "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        "sec-ch-ua-mobile" to "?0",
        "sec-ch-ua-platform" to "\"Windows\"",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )
    private val SENDER_RE = Regex("(?:发件人|From)\\s*[:：]\\s*(.+?)(?:<br|</|<p|\\n|\\r)", RegexOption.IGNORE_CASE)
    private val SUBJECT_RE = Regex("(?:主题|Subject)\\s*[:：]\\s*(.+?)(?:<br|</|<p|\\n|\\r)", RegexOption.IGNORE_CASE)

    /** 创建临时邮箱（随机选取域名）。 */
    override suspend fun generate(): EmailInfo {
        val domains = fetchDomains()
        if (domains.isEmpty()) throw RuntimeException("tempmail365: 无可用域名")
        val selected = domains.random()
        val addr = "${ProviderUtil.randomString(8)}@$selected"
        val resp = ProviderUtil.httpGet(
            "$BASE?action=create_email&email=${ProviderUtil.urlEncode(addr)}" +
                "&domain=${ProviderUtil.urlEncode(selected)}", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("tempmail365: 创建邮箱失败")
        if ((root["success"] as? JsonPrimitive)?.booleanOrNull != true) {
            throw RuntimeException("tempmail365: 创建邮箱失败")
        }
        return EmailInfo(email = addr, channel = "tempmail365")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val e = info.email.trim()
        if (e.isBlank()) throw RuntimeException("tempmail365: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE?action=fetch_mail&email=${ProviderUtil.urlEncode(e)}", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val content = jsonStr(root, "content")
        if (content.isBlank() || content.trim() == "无邮件") return emptyList()
        val flat = mapOf(
            "from" to (SENDER_RE.find(content)?.groupValues?.get(1)?.trim() ?: ""),
            "subject" to (SUBJECT_RE.find(content)?.groupValues?.get(1)?.trim() ?: ""),
            "body" to content,
            "date" to DateTimeFormatter.ISO_INSTANT.format(Instant.now()),
        )
        return listOf(Normalize.fromMap(flat, e))
    }

    /** 拉取可用域名列表，失败回退到内置列表。 */
    private suspend fun fetchDomains(): List<String> {
        return try {
            val resp = ProviderUtil.httpGet("$BASE?action=get_config", HEADERS)
            resp.ensureSuccess()
            val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return FALLBACK_DOMAINS
            val arr = root["domains"] as? JsonArray ?: return FALLBACK_DOMAINS
            val out = arr.mapNotNull { (it as? JsonPrimitive)?.contentOrNull?.trim() }.filter { it.isNotEmpty() }
            out.ifEmpty { FALLBACK_DOMAINS }
        } catch (_: Exception) {
            FALLBACK_DOMAINS
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
