package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * TempGmailer 渠道（tempgmailer.com）。
 *
 * GET 首页获取 CSRF + Laravel session，POST /get-gmail 创建，POST /get-inbox 收信。
 * token 为 JSON {"csrfToken","cookies"}。
 */
object TempGmailer : Provider {

    private const val CHANNEL = "tempgmailer"
    private const val BASE_URL = "https://tempgmailer.com"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

    private fun apiHeaders(csrf: String, cookie: String) = mapOf(
        "User-Agent" to UA,
        "Accept" to "application/json, text/plain, */*",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer" to "$BASE_URL/",
        "X-Requested-With" to "XMLHttpRequest",
        "X-TempGmailer-Auth" to "frontend",
        "Content-Type" to "application/json",
        "X-CSRF-TOKEN" to csrf,
        "Cookie" to cookie,
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        // 获取首页 CSRF + session cookie
        val pageH = mapOf(
            "User-Agent" to UA,
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        )
        val resp = ProviderUtil.httpGet(BASE_URL, pageH)
        resp.ensureSuccess()
        val csrf = ProviderUtil.extractCsrfToken(resp.body)
        if (csrf.isEmpty()) throw RuntimeException("tempgmailer: 无法提取 CSRF token")
        var cookie = ProviderUtil.extractAllCookies(resp.setCookies)
        if (cookie.isEmpty()) throw RuntimeException("tempgmailer: 未获取到 session cookie")
        // POST /get-gmail
        val body = "{\"refresh\":true,\"adblock\":0}"
        val resp2 = ProviderUtil.httpPost("$BASE_URL/get-gmail", body, "application/json", apiHeaders(csrf, cookie))
        resp2.ensureSuccess()
        val newCk = ProviderUtil.extractAllCookies(resp2.setCookies)
        if (newCk.isNotEmpty()) cookie = ProviderUtil.mergeCookieStrings(cookie, newCk)
        val data = ProviderUtil.parseObject(resp2.body)
        if (!ProviderUtil.bool(data, "success")) throw RuntimeException("tempgmailer: 创建邮箱失败")
        val address = ProviderUtil.str(data?.get("data") as? JsonObject, "email").trim()
        if (address.isEmpty() || !address.contains("@")) throw RuntimeException("tempgmailer: 未返回邮箱地址")
        val token = buildJsonObject {
            put("csrfToken", csrf)
            put("cookies", cookie)
        }.toString()
        return EmailInfo(email = address, channel = CHANNEL, token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("tempgmailer: 邮箱地址为空")
        val tokenObj = ProviderUtil.parseObject(info.token.ifBlank { "{}" })
        val csrf = ProviderUtil.str(tokenObj, "csrfToken").trim()
        val cookie = ProviderUtil.str(tokenObj, "cookies").trim()
        if (csrf.isEmpty() || cookie.isEmpty()) throw RuntimeException("tempgmailer: token 无效")
        val body = "{\"email\":\"$address\",\"adblock\":0}"
        val resp = ProviderUtil.httpPost("$BASE_URL/get-inbox", body, "application/json", apiHeaders(csrf, cookie))
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        if (!ProviderUtil.bool(data, "success")) return emptyList()
        val dataObj = data?.get("data") as? JsonObject ?: return emptyList()
        val messages = ProviderUtil.arr(dataObj, "messages") ?: return emptyList()
        return messages.filterIsInstance<JsonObject>().map { item ->
            // from 可能是对象或字符串
            val fromField = when (val fromEl = item["from"]) {
                is JsonObject -> ProviderUtil.str(fromEl, "address")
                is JsonPrimitive -> fromEl.content
                else -> ""
            }
            val row = mapOf(
                "id" to ProviderUtil.str(item, "id"),
                "from" to fromField,
                "to" to address,
                "subject" to ProviderUtil.str(item, "subject"),
                "text" to ProviderUtil.str(item, "intro"),
                "html" to ProviderUtil.firstNonEmpty(ProviderUtil.str(item, "body"), ProviderUtil.str(item, "intro")),
                "date" to ProviderUtil.str(item, "createdAt"),
            )
            Normalize.fromMap(row, address)
        }
    }
}
