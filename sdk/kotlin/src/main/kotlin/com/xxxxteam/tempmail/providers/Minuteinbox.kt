package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Minuteinbox 渠道（www.minuteinbox.com）。
 *
 * GET / 取 PHPSESSID + CSRF，GET /index/index 创建邮箱，
 * GET /index/refresh 收信，POST /index/email 取详情。token 存储 JSON {csrf, cookies}。
 */
object Minuteinbox : Provider {

    private const val ORIGIN = "https://www.minuteinbox.com"
    private val CSRF_RE = Regex("CSRF\\s*=\\s*\"([^\"]+)\"")

    private fun ajaxHeaders(cookie: String): Map<String, String> {
        val h = mutableMapOf(
            "User-Agent" to "Mozilla/5.0",
            "Accept" to "application/json, text/javascript, */*; q=0.01",
            "X-Requested-With" to "XMLHttpRequest",
            "Referer" to "$ORIGIN/",
        )
        if (cookie.isNotEmpty()) h["Cookie"] = cookie
        return h
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val r1 = ProviderUtil.httpGet("$ORIGIN/",
            mapOf("Accept" to "text/html", "User-Agent" to "Mozilla/5.0"))
        r1.ensureSuccess()
        var cookie = mergeCookies("", r1)
        val csrf = CSRF_RE.find(r1.body)?.groupValues?.get(1)
            ?: throw RuntimeException("minuteinbox: 无法提取 CSRF token")
        val r2 = ProviderUtil.httpGet(
            "$ORIGIN/index/index?csrf_token=${ProviderUtil.urlEncode(csrf)}", ajaxHeaders(cookie))
        r2.ensureSuccess()
        cookie = mergeCookies(cookie, r2)
        val data = Http.json.parseToJsonElement(r2.body) as? JsonObject
            ?: throw RuntimeException("minuteinbox: 响应格式异常")
        val email = jsonStr(data, "email").trim()
        if (email.isBlank() || !email.contains("@")) {
            throw RuntimeException("minuteinbox: 返回的邮箱地址无效")
        }
        val token = buildJsonObject { put("csrf", csrf); put("cookies", cookie) }.toString()
        return EmailInfo(email = email, channel = "minuteinbox", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val email = info.email.trim()
        if (email.isBlank()) throw RuntimeException("minuteinbox: 邮箱地址为空")
        val tokObj = Http.json.parseToJsonElement(info.token) as? JsonObject
            ?: throw RuntimeException("minuteinbox: token 格式无效")
        val cookie = jsonStr(tokObj, "cookies")
        val resp = ProviderUtil.httpGet("$ORIGIN/index/refresh", ajaxHeaders(cookie))
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { row ->
            val mailId = jsonStr(row, "id")
            val bodyHtml = if (mailId.isNotEmpty()) fetchDetail(mailId, cookie) else ""
            val flat = mapOf(
                "id" to mailId,
                "from" to jsonStr(row, "od"),
                "to" to email,
                "subject" to jsonStr(row, "predmet"),
                "html" to bodyHtml,
                "date" to jsonStr(row, "kdy"),
            )
            Normalize.fromMap(flat, email)
        }
    }

    /** 拉取单封邮件正文（telo 字段）。 */
    private suspend fun fetchDetail(mailId: String, cookie: String): String {
        return try {
            val dh = ajaxHeaders(cookie) + ("Content-Type" to "application/x-www-form-urlencoded")
            val dr = ProviderUtil.httpPost("$ORIGIN/index/email",
                "id=${ProviderUtil.urlEncode(mailId)}", "application/x-www-form-urlencoded", dh)
            if (!dr.isOk) return ""
            val detail = Http.json.parseToJsonElement(dr.body) as? JsonObject ?: return ""
            jsonStr(detail, "telo")
        } catch (_: Exception) {
            ""
        }
    }

    /** 合并 cookie 串与响应新 cookie。 */
    private fun mergeCookies(prev: String, resp: HttpResp): String {
        val jar = LinkedHashMap<String, String>()
        prev.split(";").forEach { part ->
            val p = part.trim()
            val eq = p.indexOf('=')
            if (eq > 0) jar[p.substring(0, eq).trim()] = p.substring(eq + 1).trim()
        }
        resp.setCookies.forEach { sc ->
            val seg = sc.split(";")[0].trim()
            val eq = seg.indexOf('=')
            if (eq > 0) jar[seg.substring(0, eq)] = seg.substring(eq + 1)
        }
        return jar.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
