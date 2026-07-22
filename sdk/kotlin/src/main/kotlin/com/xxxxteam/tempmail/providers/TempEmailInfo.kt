package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * tempemail.info 渠道实现 — https://tempemail.info
 *
 * GET / 获取 PHPSESSID + HTML 中 base64 编码的邮箱地址（var emailEncoded），
 * POST /template/checker.php 获取邮件列表，GET /view/{date} 获取正文。
 * token 为 session cookie 字符串。
 */
object TempEmailInfo : Provider {

    private const val BASE_URL = "https://tempemail.info"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val headers = mapOf(
            "User-Agent" to UA,
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
            "Referer" to "$BASE_URL/",
        )
        val resp = ProviderUtil.httpGet("$BASE_URL/", headers)
        resp.ensureSuccess()
        val cookie = ProviderUtil.extractAllCookies(resp)
        if (cookie.isEmpty()) throw RuntimeException("tempemail-info: 未获取到会话 Cookie")
        val encoded = ProviderUtil.regexExtract(resp.body, "var\\s+emailEncoded\\s*=\\s*\"([^\"]+)\"")
        if (encoded.isEmpty()) throw RuntimeException("tempemail-info: 未在页面中找到 emailEncoded")
        val address = try {
            String(ProviderUtil.base64Decode(encoded), Charsets.UTF_8).trim()
        } catch (_: Exception) {
            throw RuntimeException("tempemail-info: 邮箱地址 base64 解码失败")
        }
        if (address.isEmpty() || !address.contains("@")) {
            throw RuntimeException("tempemail-info: 解码出的邮箱地址无效")
        }
        return EmailInfo(email = address, channel = "tempemail-info", token = cookie)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        val cookie = info.token.trim()
        if (cookie.isEmpty()) throw RuntimeException("tempemail-info: 缺少会话 Cookie")
        val headers = mapOf(
            "User-Agent" to UA,
            "Accept" to "application/json, text/javascript, */*; q=0.01",
            "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
            "X-Requested-With" to "XMLHttpRequest",
            "Content-Type" to "application/x-www-form-urlencoded",
            "Cookie" to cookie,
            "Referer" to "$BASE_URL/",
            "Origin" to BASE_URL,
        )
        val resp = ProviderUtil.httpPost(
            "$BASE_URL/template/checker.php", "last_id=0", "application/x-www-form-urlencoded", headers)
        resp.ensureSuccess()
        val rows = ProviderUtil.parse(resp.body) as? JsonArray ?: return emptyList()
        val result = ArrayList<Email>(rows.size)
        for (row in rows) {
            val msg = row as? JsonObject ?: continue
            var fromAddr = ProviderUtil.str(msg, "from")
            val name = ProviderUtil.str(msg, "name")
            if (name.isNotEmpty() && name != fromAddr) fromAddr = "$name <$fromAddr>"
            val date = ProviderUtil.str(msg, "date")
            val raw = mapOf(
                "id" to ProviderUtil.str(msg, "id"),
                "from" to fromAddr,
                "to" to address,
                "subject" to ProviderUtil.str(msg, "subject"),
                "date" to date,
                "html" to fetchBody(cookie, date),
            )
            result.add(Normalize.fromMap(raw, address))
        }
        return result
    }

    /** 拉取单封邮件 HTML 正文，失败返回空串。 */
    private suspend fun fetchBody(cookie: String, date: String): String {
        if (date.trim().isEmpty()) return ""
        return try {
            val headers = mapOf(
                "User-Agent" to UA,
                "Accept" to "text/html,*/*",
                "Cookie" to cookie,
                "Referer" to "$BASE_URL/",
            )
            val resp = ProviderUtil.httpGet("$BASE_URL/view/${ProviderUtil.urlEncode(date.trim())}", headers)
            if (resp.isOk) resp.body else ""
        } catch (_: Exception) {
            ""
        }
    }
}
