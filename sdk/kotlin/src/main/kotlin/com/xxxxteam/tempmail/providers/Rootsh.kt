package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import com.xxxxteam.tempmail.providers.ProviderUtil.str
import kotlinx.serialization.json.*

/**
 * Rootsh(BccTo) 渠道（rootsh.com）。
 *
 * GET / 获取 session cookie，POST /applymail 创建，POST /getmail 收信，POST /viewmail 取正文。
 * token 为 "rootsh1:" + JSON {t: lastCheckTime, c: cookie}。
 */
object Rootsh : Provider {

    private const val BASE_URL = "https://rootsh.com"
    private const val TOK_PREFIX = "rootsh1:"
    private const val DEFAULT_DOMAIN = "bccto.cc"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val local = ProviderUtil.randomString(10)
        val mailAddr = "$local@$DEFAULT_DOMAIN"

        val resp = ProviderUtil.httpGet("$BASE_URL/", mapOf(
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "User-Agent" to UA,
        ))
        resp.ensureSuccess()
        var cookie = extractAllCookies(resp.setCookies)

        val body = "mail=${ProviderUtil.urlEncode(mailAddr)}"
        val resp2 = ProviderUtil.httpPost(
            "$BASE_URL/applymail", body, "application/x-www-form-urlencoded", apiHeaders(cookie))
        resp2.ensureSuccess()
        val newCk = extractAllCookies(resp2.setCookies)
        if (newCk.isNotEmpty()) cookie = mergeCookieStrings(cookie, newCk)

        val data = ProviderUtil.parseObject(resp2.body)
        if (str(data, "success") != "true") {
            throw RuntimeException("rootsh: 邮箱申请失败: ${str(data, "tips")}")
        }
        val actualEmail = str(data, "user").trim().ifEmpty { mailAddr }
        val lastCheckTime = (data?.get("time") as? JsonPrimitive)?.longOrNull ?: 0L
        return EmailInfo(email = actualEmail, channel = "rootsh", token = encodeToken(lastCheckTime, cookie))
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("rootsh: 邮箱地址为空")
        val (lastCheckTime, cookie) = decodeToken(info.token)
        val ts = System.currentTimeMillis()

        val body = "mail=${ProviderUtil.urlEncode(address)}&time=$lastCheckTime&_=$ts"
        val resp = ProviderUtil.httpPost(
            "$BASE_URL/getmail", body, "application/x-www-form-urlencoded", apiHeaders(cookie))
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        if (str(data, "success") != "true") return emptyList()
        val mailList = ProviderUtil.arr(data, "mail") ?: return emptyList()

        val result = ArrayList<Email>()
        for (item in mailList) {
            val arr = item as? JsonArray ?: continue
            if (arr.size < 5) continue
            val fromEmail = (arr[1] as? JsonPrimitive)?.contentOrNull ?: ""
            val subject = (arr[2] as? JsonPrimitive)?.contentOrNull ?: ""
            val dateStr = (arr[3] as? JsonPrimitive)?.contentOrNull ?: ""
            val mailFid = (arr[4] as? JsonPrimitive)?.contentOrNull ?: ""
            val htmlContent = fetchViewMail(cookie, mailFid, address)
            val flat = mapOf(
                "id" to mailFid,
                "from" to fromEmail,
                "to" to address,
                "subject" to subject,
                "date" to dateStr,
                "html" to htmlContent,
            )
            result.add(Normalize.fromMap(flat, address))
        }
        return result
    }

    /** POST /viewmail 获取正文 HTML，失败返回空串。 */
    private suspend fun fetchViewMail(cookie: String, mailFid: String, toAddr: String): String {
        if (mailFid.isEmpty()) return ""
        return try {
            val body = "mail=${ProviderUtil.urlEncode(mailFid)}&to=${ProviderUtil.urlEncode(toAddr)}"
            val resp = ProviderUtil.httpPost(
                "$BASE_URL/viewmail", body, "application/x-www-form-urlencoded", apiHeaders(cookie))
            if (!resp.isOk) return ""
            val data = ProviderUtil.parseObject(resp.body)
            if (str(data, "success") == "true") str(data, "mail") else ""
        } catch (_: Exception) {
            ""
        }
    }

    /** 构造带 cookie 的 API 请求头。 */
    private fun apiHeaders(cookie: String): Map<String, String> = mapOf(
        "Accept" to "application/json, text/plain, */*",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer" to "$BASE_URL/",
        "X-Requested-With" to "XMLHttpRequest",
        "Content-Type" to "application/x-www-form-urlencoded; charset=UTF-8",
        "User-Agent" to UA,
        "Cookie" to cookie,
    )

    /** 编码 token。 */
    private fun encodeToken(lastCheckTime: Long, cookie: String): String =
        TOK_PREFIX + buildJsonObject {
            put("t", lastCheckTime)
            put("c", cookie)
        }.toString()

    /** 解码 token，返回 (lastCheckTime, cookie)。 */
    private fun decodeToken(token: String): Pair<Long, String> {
        if (!token.startsWith(TOK_PREFIX)) throw RuntimeException("rootsh: 无效的 session token")
        val obj = ProviderUtil.parseObject(token.substring(TOK_PREFIX.length))
        val t = (obj?.get("t") as? JsonPrimitive)?.longOrNull ?: 0L
        return t to str(obj, "c").trim()
    }

    /** 将 Set-Cookie 列表拼接为 Cookie 请求头。 */
    private fun extractAllCookies(setCookies: List<String>): String =
        setCookies.map { it.substringBefore(";").trim() }
            .filter { it.isNotEmpty() && it.contains("=") }
            .joinToString("; ")

    /** 合并两个 Cookie 字符串，后者覆盖同名键。 */
    private fun mergeCookieStrings(prev: String, next: String): String {
        val cookies = LinkedHashMap<String, String>()
        for (segment in listOf(prev, next)) {
            for (part in segment.split(";")) {
                val nv = part.trim()
                val eq = nv.indexOf('=')
                if (eq > 0) cookies[nv.substring(0, eq).trim()] = nv.substring(eq + 1).trim()
            }
        }
        return cookies.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }
}
