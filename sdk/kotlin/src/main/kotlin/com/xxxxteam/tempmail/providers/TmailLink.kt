package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * tmail.link 渠道 — https://tmail.link
 *
 * GET / 正则提取邮箱，GET /inbox/{email}/ 获取 csrftoken cookie，
 * POST /inbox/{email}/ form: format=json&csrfmiddlewaretoken={token} 收信。
 * token 存 csrftoken 值。
 */
object TmailLink : Provider {

    private const val BASE_URL = "https://tmail.link"
    private const val UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
    private val EMAIL_RE = Regex("([a-zA-Z0-9._%+-]+@tmail\\.link)")

    /** 创建临时邮箱，token 为 csrftoken 值。 */
    override suspend fun generate(): EmailInfo {
        val h = mapOf(
            "User-Agent" to UA,
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        )
        val resp = ProviderUtil.httpGet("$BASE_URL/", h)
        resp.ensureSuccess()
        val address = EMAIL_RE.find(resp.body)?.groupValues?.get(1)
            ?: throw RuntimeException("tmail-link: 未能从首页提取邮箱地址")
        val resp2 = ProviderUtil.httpGet("$BASE_URL/inbox/$address/", h)
        resp2.ensureSuccess()
        var csrfToken = ""
        for (raw in resp2.setCookies) {
            val idx = raw.indexOf("csrftoken=")
            if (idx >= 0) {
                val seg = raw.substring(idx + "csrftoken=".length)
                val semi = seg.indexOf(';')
                csrfToken = if (semi < 0) seg else seg.substring(0, semi)
                break
            }
        }
        if (csrfToken.isEmpty()) throw RuntimeException("tmail-link: 未能获取 csrftoken")
        return EmailInfo(email = address, channel = "tmail-link", token = csrfToken)
    }
    /** 获取邮件列表，token 为 csrftoken 值。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        val csrf = info.token.trim()
        if (address.isEmpty()) throw RuntimeException("tmail-link: 邮箱地址为空")
        if (csrf.isEmpty()) throw RuntimeException("tmail-link: token 为空")
        val h = mapOf(
            "User-Agent" to UA,
            "Accept" to "application/json, text/javascript, */*; q=0.01",
            "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
            "X-Requested-With" to "XMLHttpRequest",
            "Content-Type" to "application/x-www-form-urlencoded; charset=UTF-8",
            "Origin" to BASE_URL,
            "Referer" to "$BASE_URL/inbox/$address/",
            "Cookie" to "csrftoken=$csrf",
        )
        val body = "format=json&csrfmiddlewaretoken=" + ProviderUtil.urlEncode(csrf)
        val resp = ProviderUtil.httpPost(
            "$BASE_URL/inbox/$address/", body, "application/x-www-form-urlencoded", h)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val messages = root["messages"] as? JsonArray ?: return emptyList()
        val result = ArrayList<Email>(messages.size)
        for (item in messages) {
            val msg = item as? JsonObject ?: continue
            val row = mapOf(
                "id" to str(msg, "key"),
                "from" to str(msg, "sender"),
                "to" to address,
                "subject" to str(msg, "subject"),
                "date" to str(msg, "date"),
            )
            result.add(Normalize.fromMap(row, address))
        }
        return result
    }

    private fun str(obj: JsonObject, key: String): String =
        (obj[key] as? JsonPrimitive)?.content ?: ""
}
