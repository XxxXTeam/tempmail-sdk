package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import com.xxxxteam.tempmail.providers.ProviderUtil.str
import kotlinx.serialization.json.*

/**
 * Smailpro 渠道（smailpro.com）。
 *
 * 两段式流程：先从 smailpro.com/app/payload 获取 JWT，再携带 JWT 调用 api.sonjj.com 接口。
 * 邮箱地址即 token（token 不参与收信逻辑）。
 */
object Smailpro : Provider {

    private const val BASE_URL = "https://smailpro.com"
    private const val API_BASE_URL = "https://api.sonjj.com/v1/temp_email"
    private val HEADERS = mapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept" to "application/json, text/plain, */*",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8",
        "Referer" to "$BASE_URL/",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val data = callApi("$API_BASE_URL/create", emptyMap())
        val email = str(data, "email").trim()
        if (email.isEmpty()) throw RuntimeException("smailpro: 创建邮箱失败，未返回邮箱地址")
        val expiresAt = str(data, "expired_at")
        return EmailInfo(email = email, channel = "smailpro", token = email, expiresAt = expiresAt)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        if (addr.isEmpty()) throw RuntimeException("smailpro: 邮箱地址为空")
        val data = callApi("$API_BASE_URL/inbox", mapOf("email" to addr))

        val messages = (data["data"] as? JsonObject)?.let { ProviderUtil.arr(it, "messages") }
            ?: ProviderUtil.arr(data, "messages")
            ?: return emptyList()

        val result = ArrayList<Email>()
        for (item in messages) {
            val m = item as? JsonObject ?: continue
            val mid = str(m, "mid").trim()
            val (html, text) = fetchMessageBody(addr, mid)
            val flat = HashMap<String, Any?>()
            flat["id"] = mid
            flat["from"] = str(m, "from")
            flat["to"] = addr
            flat["subject"] = str(m, "subject")
            flat["date"] = str(m, "datetime")
            if (html.isNotEmpty() || text.isNotEmpty()) {
                flat["html"] = html
                flat["text"] = text
            }
            result.add(Normalize.fromMap(flat, addr))
        }
        return result
    }

    /** 通过 payload 端点换取 JWT，再调用目标 API 返回 JsonObject。 */
    private suspend fun callApi(targetUrl: String, extra: Map<String, String>): JsonObject {
        val payload = fetchPayload(targetUrl, extra)
        val resp = ProviderUtil.httpGet("$targetUrl?payload=${ProviderUtil.urlEncode(payload)}", HEADERS)
        resp.ensureSuccess()
        return ProviderUtil.parseObject(resp.body)
            ?: throw RuntimeException("smailpro: 无效的 API 响应")
    }

    /** 获取 JWT payload。 */
    private suspend fun fetchPayload(targetUrl: String, extra: Map<String, String>): String {
        val sb = StringBuilder("$BASE_URL/app/payload?url=${ProviderUtil.urlEncode(targetUrl)}")
        for ((k, v) in extra) {
            sb.append('&').append(ProviderUtil.urlEncode(k)).append('=').append(ProviderUtil.urlEncode(v))
        }
        val resp = ProviderUtil.httpGet(sb.toString(), HEADERS)
        resp.ensureSuccess()
        val payload = resp.body.trim().replace("\"", "")
        if (payload.isEmpty()) throw RuntimeException("smailpro: payload 为空")
        return payload
    }

    /** 拉取单封邮件正文，返回 (html, text)，失败返回空串对。 */
    private suspend fun fetchMessageBody(email: String, mid: String): Pair<String, String> {
        if (mid.isEmpty()) return "" to ""
        return try {
            val data = callApi("$API_BASE_URL/message", mapOf("email" to email, "mid" to mid))
            str(data, "body") to str(data, "textBody")
        } catch (_: Exception) {
            "" to ""
        }
    }
}
