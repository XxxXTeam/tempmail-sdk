package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.util.Base64
import kotlin.random.Random

/**
 * 10minutemail.one 渠道实现。
 *
 * SSR __NUXT_DATA__ 中的 mailServiceToken（JWT）+ 页面 emailDomains；
 * 收信 GET web.10minutemail.one/api/v1/mailbox/{email}。
 *
 * 域名别名：xghff.com / oqqaj.com / psovv.com / dbwot.com / ygwpr.com / imxwe.com
 *
 * @property channel 对外渠道标识
 * @property domain 指定域名，可为空
 */
class TenminuteOne(private val channel: String, private val domain: String) : Provider {

    /** 创建临时邮箱（token 存储 JWT）。 */
    override suspend fun generate(): EmailInfo {
        val pageUrl = "$SITE_ORIGIN/zh"
        val pageHeaders = mapOf(
            "User-Agent" to DEFAULT_UA,
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
            "Cache-Control" to "no-cache",
            "Pragma" to "no-cache",
            "DNT" to "1",
            "Referer" to pageUrl,
        )
        val r = ProviderUtil.httpGet(pageUrl, pageHeaders)
        r.ensureSuccess()
        val html = r.body

        val token = parseMailServiceToken(html)
        val domains = parseEmailDomains(html).ifEmpty { KNOWN_DOMAINS.toList() }

        val domHint = domain.trim().takeIf { it.contains(".") }
        val mailDomain = if (domHint != null) {
            domains.firstOrNull { it.equals(domHint, ignoreCase = true) } ?: domains[Random.nextInt(domains.size)]
        } else {
            domains[Random.nextInt(domains.size)]
        }

        val address = "${ProviderUtil.randomString(10)}@$mailDomain"
        val expMs = jwtExpMs(token)
        return EmailInfo(email = address, channel = channel, token = token, expiresAt = expMs?.toString() ?: "")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val email = info.email
        val token = info.token
        val url = "$API_BASE/mailbox/${encMailboxEmail(email)}"
        val r = ProviderUtil.httpGet(url, apiHeaders(token))
        r.ensureSuccess()
        val data = ProviderUtil.parse(r.body) as? JsonArray
            ?: throw RuntimeException("10minute-one: invalid inbox JSON")

        val result = mutableListOf<Email>()
        for (raw in data.filterIsInstance<JsonObject>()) {
            val row = raw.toMutableMap()
            if (needsDetail(raw)) {
                val mid = ProviderUtil.str(raw, "id")
                if (mid.isNotEmpty()) {
                    try {
                        val du = "$API_BASE/mailbox/${encMailboxEmail(email)}/${ProviderUtil.urlEncode(mid)}"
                        val d = ProviderUtil.httpGet(du, apiHeaders(token))
                        if (d.isOk) {
                            ProviderUtil.parseObject(d.body)?.forEach { (k, v) -> row.putIfAbsent(k, v) }
                        }
                    } catch (_: Exception) {
                    }
                }
            }
            result.add(Normalize.fromJson(JsonObject(row), email))
        }
        return result
    }

    private fun needsDetail(m: JsonObject): Boolean {
        if (ProviderUtil.str(m, "id").trim().isEmpty()) return false
        val body = ProviderUtil.firstNonEmpty(
            ProviderUtil.str(m, "text"), ProviderUtil.str(m, "body"),
            ProviderUtil.str(m, "html"), ProviderUtil.str(m, "mail_text"),
        )
        return body.isEmpty()
    }

    private fun encMailboxEmail(email: String): String {
        if (email.contains("@")) {
            val parts = email.split("@", limit = 2)
            return "${ProviderUtil.urlEncode(parts[0])}%40${ProviderUtil.urlEncode(parts[1])}"
        }
        return ProviderUtil.urlEncode(email)
    }

    private fun apiHeaders(token: String): Map<String, String> = mapOf(
        "Accept" to "*/*",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        "Authorization" to "Bearer $token",
        "Cache-Control" to "no-cache",
        "Content-Type" to "application/json",
        "DNT" to "1",
        "Origin" to SITE_ORIGIN,
        "Pragma" to "no-cache",
        "Referer" to "$SITE_ORIGIN/",
        "User-Agent" to DEFAULT_UA,
        "X-Request-ID" to ProviderUtil.randomString(32),
        "X-Timestamp" to (System.currentTimeMillis() / 1000).toString(),
    )

    private fun parseMailServiceToken(html: String): String {
        val m = nuxtDataRe.find(html) ?: throw RuntimeException("10minute-one: __NUXT_DATA__ not found")
        val arr = ProviderUtil.parse(m.groupValues[1].trim()) as? JsonArray
            ?: throw RuntimeException("10minute-one: invalid __NUXT_DATA__")

        // 查找包含 mailServiceToken 字段的对象并解析引用
        for (el in arr) {
            val obj = el as? JsonObject ?: continue
            val tokenRef = obj["mailServiceToken"] ?: continue
            val resolved = resolveRef(arr, tokenRef, 0)
            if (resolved != null && jwtRe.matches(resolved)) return resolved
        }
        // 直接找 JWT 字符串
        for (el in arr) {
            val p = el as? JsonPrimitive ?: continue
            if (p.isString && jwtRe.matches(p.content)) return p.content
        }
        throw RuntimeException("10minute-one: mailServiceToken not found")
    }

    private fun resolveRef(arr: JsonArray, value: JsonElement?, depth: Int): String? {
        if (depth > 64) return (value as? JsonPrimitive)?.content
        val p = value as? JsonPrimitive ?: return null
        if (p.isString) return p.content
        val idx = p.intOrNull ?: return null
        if (idx in 0 until arr.size) return resolveRef(arr, arr[idx], depth + 1)
        return null
    }

    private fun parseEmailDomains(html: String): List<String> {
        val key = "emailDomains:\""
        val start = html.indexOf(key)
        if (start < 0) return emptyList()
        val sliceStart = start + key.length
        if (sliceStart >= html.length || html[sliceStart] != '[') return emptyList()
        var depth = 0
        var j = sliceStart
        while (j < html.length) {
            when (html[j]) {
                '[' -> depth++
                ']' -> {
                    depth--
                    if (depth == 0) {
                        j++
                        break
                    }
                }
            }
            j++
        }
        val raw = html.substring(sliceStart, j).replace("\\\"", "\"")
        return try {
            (ProviderUtil.parse(raw) as? JsonArray)
                ?.filterIsInstance<JsonPrimitive>()?.map { it.content } ?: emptyList()
        } catch (_: Exception) {
            emptyList()
        }
    }

    private fun jwtExpMs(token: String): Long? {
        val parts = token.split(".")
        if (parts.size < 2) return null
        return try {
            var payload = parts[1]
            val pad = (4 - payload.length % 4) % 4
            payload = payload + "=".repeat(pad)
            payload = payload.replace('-', '+').replace('_', '/')
            val decoded = Base64.getDecoder().decode(payload)
            val obj = ProviderUtil.parseObject(String(decoded, Charsets.UTF_8))
            (obj?.get("exp") as? JsonPrimitive)?.longOrNull?.let { it * 1000 }
        } catch (_: Exception) {
            null
        }
    }

    companion object {
        private const val SITE_ORIGIN = "https://10minutemail.one"
        private const val API_BASE = "https://web.10minutemail.one/api/v1"
        private val KNOWN_DOMAINS = arrayOf("xghff.com", "oqqaj.com", "psovv.com", "dbwot.com", "ygwpr.com", "imxwe.com")
        private const val DEFAULT_UA =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
        private val nuxtDataRe = Regex(
            "<script[^>]*\\bid=\"__NUXT_DATA__\"[^>]*>([\\s\\S]*?)</script>",
            RegexOption.IGNORE_CASE,
        )
        private val jwtRe = Regex("^[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+$")
    }
}
