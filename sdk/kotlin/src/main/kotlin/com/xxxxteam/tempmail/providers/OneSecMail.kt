package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * 1SecMail 渠道（tmaily.com，会话 Cookie 鉴权）。
 *
 * generate 提取 TMaily_sid Cookie 作为 token，getEmails 携带该 Cookie 查询收件箱。
 */
object OneSecMail : Provider {

    private const val BASE_URL = "https://tmaily.com/"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0",
    )

    /** 创建临时邮箱，提取会话 Cookie 作为 token。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet(BASE_URL + "generate", HEADERS)
        resp.ensureSuccess()
        val cookie = extractCookie(resp.setCookies)
        if (cookie.isEmpty()) throw RuntimeException("1sec-mail: 未获取到会话 Cookie")
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject
            ?: throw RuntimeException("1sec-mail: 无效的邮箱响应")
        val email = jsonStr(root, "address").trim()
        if (!email.contains("@")) throw RuntimeException("1sec-mail: 无效的邮箱响应")
        return EmailInfo(email = email, channel = "1sec-mail", token = cookie)
    }

    /** 从 Set-Cookie 列表中提取 TMaily_sid。 */
    private fun extractCookie(setCookies: List<String>): String {
        for (c in setCookies) {
            val idx = c.indexOf("TMaily_sid=")
            if (idx < 0) continue
            val seg = c.substring(idx)
            val semi = seg.indexOf(';')
            return if (semi < 0) seg else seg.substring(0, semi)
        }
        return ""
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val address = info.email.trim()
        if (address.isEmpty()) throw RuntimeException("1sec-mail: 邮箱地址为空")
        val headers = HEADERS + ("Cookie" to info.token)
        val resp = ProviderUtil.httpGet("${BASE_URL}emails?address=${ProviderUtil.urlEncode(address)}", headers)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()
        return arr.mapNotNull { it as? JsonObject }.map { Normalize.fromJson(it, address) }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
