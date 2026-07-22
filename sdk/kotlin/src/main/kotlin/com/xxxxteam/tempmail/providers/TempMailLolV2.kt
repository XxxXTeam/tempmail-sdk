package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * tempmail.lol V1 渠道（GET /generate）。
 *
 * token 存创建时返回的会话令牌。
 */
object TempMailLolV2 : Provider {

    private const val API_BASE = "https://api.tempmail.lol"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpGet("$API_BASE/generate", HEADERS)
        resp.ensureSuccess()
        val data = ProviderUtil.parseObject(resp.body)
        val address = ProviderUtil.str(data, "address")
        val token = ProviderUtil.str(data, "token")
        if (address.isEmpty() || token.isEmpty()) {
            throw RuntimeException("tempmail-lol-v2: missing address or token")
        }
        return EmailInfo(email = address, channel = "tempmail-lol-v2", token = token)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val email = info.email
        val resp = ProviderUtil.httpGet(
            "$API_BASE/auth/" + ProviderUtil.urlEncode(info.token), HEADERS)
        resp.ensureSuccess()
        val list = ProviderUtil.arr(ProviderUtil.parseObject(resp.body), "email") ?: return emptyList()
        val result = ArrayList<Email>(list.size)
        for (item in list) {
            val raw = item as? JsonObject ?: continue
            val body = ProviderUtil.str(raw, "body")
            val dict = mapOf(
                "id" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "id"), ProviderUtil.str(raw, "_id")),
                "from" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "from"), ProviderUtil.str(raw, "sender")),
                "to" to email,
                "subject" to ProviderUtil.str(raw, "subject"),
                "text" to ProviderUtil.firstNonEmpty(body, ProviderUtil.str(raw, "text")),
                "html" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "html"), body),
                "date" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "date"), ProviderUtil.str(raw, "receivedAt")),
            )
            result.add(Normalize.fromMap(dict, email))
        }
        return result
    }
}
