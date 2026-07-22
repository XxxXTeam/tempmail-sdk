package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * FakeMail 渠道（www.fakemail.net）。
 *
 * GET / 获取 CSRF → GET /index/index 创建邮箱 → GET /index/refresh 获取列表
 * → POST /index/email 获取详情。token 为会话 Cookie。
 */
object Fakemail : Provider {

    private const val BASE_URL = "https://www.fakemail.net"
    private val CSRF_RE = Regex("const\\s+CSRF\\s*=\\s*\"([^\"]+)\"")

    /** 构造 AJAX 请求头。 */
    private fun ajaxHeaders(cookie: String): Map<String, String> {
        val h = linkedMapOf(
            "Accept" to "application/json, text/javascript, */*; q=0.01",
            "X-Requested-With" to "XMLHttpRequest",
            "Referer" to "$BASE_URL/",
            "User-Agent" to "Mozilla/5.0",
        )
        if (cookie.isNotEmpty()) h["Cookie"] = cookie
        return h
    }

    /** 合并旧 Cookie 与响应中的 Set-Cookie。 */
    private fun mergeCookies(prev: String, setCookies: List<String>): String {
        val jar = LinkedHashMap<String, String>()
        for (part in prev.split(";")) {
            val p = part.trim()
            val eq = p.indexOf('=')
            if (eq > 0) jar[p.substring(0, eq).trim()] = p.substring(eq + 1).trim()
        }
        for (sc in setCookies) {
            val seg = sc.split(";")[0].trim()
            val eq = seg.indexOf('=')
            if (eq > 0) jar[seg.substring(0, eq)] = seg.substring(eq + 1)
        }
        return jar.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val bh = mapOf(
            "Accept" to "text/html,application/xhtml+xml",
            "User-Agent" to "Mozilla/5.0",
        )
        val r1 = ProviderUtil.httpGet("$BASE_URL/", bh)
        r1.ensureSuccess()
        val csrf = CSRF_RE.find(r1.body)?.groupValues?.get(1)
            ?: throw RuntimeException("fakemail: 未找到 csrf token")
        var cookie = mergeCookies("", r1.setCookies)

        val r2 = ProviderUtil.httpGet(
            "$BASE_URL/index/index?csrf_token=${ProviderUtil.urlEncode(csrf)}", ajaxHeaders(cookie),
        )
        r2.ensureSuccess()
        cookie = mergeCookies(cookie, r2.setCookies)

        val data = Http.json.parseToJsonElement(r2.body) as? JsonObject
            ?: throw RuntimeException("fakemail: 邮箱响应无效")
        val email = jsonStr(data, "email").trim()
        if (email.isEmpty() || !email.contains("@")) throw RuntimeException("fakemail: 邮箱响应无效")
        return EmailInfo(email = email, channel = "fakemail", token = cookie)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token
        val email = info.email.trim()
        if (token.isBlank()) throw RuntimeException("fakemail: 会话 token 为空")
        if (email.isBlank()) throw RuntimeException("fakemail: 邮箱地址为空")
        val resp = ProviderUtil.httpGet("$BASE_URL/index/refresh", ajaxHeaders(token))
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { row ->
            val msgId = jsonStr(row, "id").trim()
            val detail = if (msgId.isNotEmpty()) fetchDetail(token, msgId) else null
            val src = detail ?: row
            val flat = mapOf(
                "id" to jsonStr(src, "id"),
                "from" to jsonStr(src, "od"),
                "to" to email,
                "subject" to jsonStr(src, "predmet"),
                "body" to if (detail != null) jsonStr(detail, "telo") else "",
                "date" to jsonStr(row, "kdy"),
            )
            Normalize.fromMap(flat, email)
        }
    }

    /** 拉取单封邮件详情。 */
    private suspend fun fetchDetail(token: String, msgId: String): JsonObject? {
        return try {
            val dh = ajaxHeaders(token) + ("Content-Type" to "application/x-www-form-urlencoded")
            val dr = ProviderUtil.httpPost(
                "$BASE_URL/index/email", "id=${ProviderUtil.urlEncode(msgId)}",
                "application/x-www-form-urlencoded", dh,
            )
            if (!dr.isOk) return null
            Http.json.parseToJsonElement(dr.body) as? JsonObject
        } catch (_: Exception) {
            null
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
