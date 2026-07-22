package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * xkx.me 渠道 — https://xkx.me
 *
 * CSRF + session cookie 认证，POST /mailbox/create/random 创建（302 redirect），
 * GET /mailbox/{email}/messages 收信。token 存 session cookie 串。
 */
object XkxMe : Provider {

    private const val BASE_URL = "https://xkx.me"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"

    /** 创建临时邮箱，token 为 session cookie 串。 */
    override suspend fun generate(): EmailInfo {
        val h = mapOf(
            "User-Agent" to UA,
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        )
        val resp = ProviderUtil.httpGet(BASE_URL, h)
        resp.ensureSuccess()
        val csrf = CookieHelpers.regexExtract(resp.body, "csrf-token\"\\s+content=\"([^\"]+)\"")
        if (csrf.isEmpty()) throw RuntimeException("xkx-me: 无法获取 CSRF token")
        var cookie = CookieHelpers.extractAllCookies(resp)
        if (cookie.isEmpty()) throw RuntimeException("xkx-me: 未获取到 session cookie")

        val postH = mapOf(
            "User-Agent" to UA,
            "Content-Type" to "application/x-www-form-urlencoded",
            "Cookie" to cookie,
            "Referer" to "$BASE_URL/",
        )
        val body = "_token=" + ProviderUtil.urlEncode(csrf)
        val resp2 = ProviderUtil.httpPost(
            "$BASE_URL/mailbox/create/random", body,
            "application/x-www-form-urlencoded", postH, followRedirects = false)
        val address = CookieHelpers.regexExtract(resp2.location, "mailbox/([^/\\s\"'<>]+@xkx\\.me)")
        if (address.isEmpty()) throw RuntimeException("xkx-me: 无法从响应中提取邮箱地址")
        val newCk = CookieHelpers.extractAllCookies(resp2)
        if (newCk.isNotEmpty()) cookie = CookieHelpers.mergeCookieStrings(cookie, newCk)
        return EmailInfo(email = address, channel = "xkx-me", token = cookie)
    }
    /** 获取邮件列表，token 为 session cookie 串。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        val cookie = info.token.trim()
        if (address.isEmpty()) throw RuntimeException("xkx-me: 邮箱地址为空")
        if (cookie.isEmpty()) throw RuntimeException("xkx-me: token 为空")
        val h = mapOf(
            "User-Agent" to UA,
            "Accept" to "application/json",
            "X-Requested-With" to "XMLHttpRequest",
            "Cookie" to cookie,
        )
        val resp = ProviderUtil.httpGet(
            "$BASE_URL/mailbox/" + ProviderUtil.urlEncode(address) + "/messages", h)
        if (resp.statusCode == 404) return emptyList()
        resp.ensureSuccess()
        val data = Http.json.parseToJsonElement(resp.body)
        // 响应可能是数组或对象
        val messageList = ArrayList<JsonObject>()
        when (data) {
            is JsonArray -> data.filterIsInstanceTo(messageList)
            is JsonObject -> {
                val msgs = data["messages"] as? JsonArray
                if (msgs != null) msgs.filterIsInstanceTo(messageList)
                else (data["message"] as? JsonObject)?.let { messageList.add(it) }
            }
            else -> {}
        }
        val result = ArrayList<Email>(messageList.size)
        for (item in messageList) {
            val row = mapOf(
                "id" to str(item, "id"),
                "from" to str(item, "from"),
                "to" to address,
                "subject" to str(item, "subject"),
                "date" to str(item, "date"),
                "html" to ProviderUtil.firstNonEmpty(str(item, "html"), str(item, "body")),
                "text" to str(item, "text"),
            )
            result.add(Normalize.fromMap(row, address))
        }
        return result
    }

    private fun str(obj: JsonObject, key: String): String =
        (obj[key] as? JsonPrimitive)?.content ?: ""
}

private fun JsonArray.filterIsInstanceTo(out: MutableList<JsonObject>) {
    for (el in this) if (el is JsonObject) out.add(el)
}
