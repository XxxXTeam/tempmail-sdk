package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Mailmomy 及其域名变体渠道（mailmomy.com）。
 *
 * 免注册、无鉴权的纯 GET JSON API，token 即邮箱地址本身。
 *
 * @property channel 对外渠道标识
 * @property domain 指定域名，为空时从活跃域名池随机取
 */
class Mailmomy(
    private val channel: String = "mailmomy",
    private val domain: String = "",
) : Provider {

    /** 从活跃域名池随机取一个，失败回退默认域名。 */
    private suspend fun pickDomain(): String {
        try {
            val resp = ProviderUtil.httpGet("$BASE_URL/api/domains/active", HEADERS)
            resp.ensureSuccess()
            val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray
            if (arr != null) {
                val domains = arr.mapNotNull { (it as? JsonPrimitive)?.takeIf { p -> p.isString }?.content }
                    .filter { it.isNotEmpty() }
                if (domains.isNotEmpty()) return domains.random()
            }
        } catch (_: Exception) {
            // 回退默认域名
        }
        return DEFAULT_DOMAIN
    }

    /** 创建邮箱。 */
    override suspend fun generate(): EmailInfo {
        val d = domain.ifEmpty { pickDomain() }
        val email = ProviderUtil.randomString(10) + "@" + d
        return EmailInfo(email = email, channel = channel, token = email)
    }
    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.token.ifBlank { info.email }.trim()
        if (addr.isEmpty()) throw RuntimeException("mailmomy: 缺少邮箱地址")
        val resp = ProviderUtil.httpGet(
            "$BASE_URL/api/mail/messages?to=" + ProviderUtil.urlEncode(addr) + "&page=1&limit=20", HEADERS)
        resp.ensureSuccess()
        val root = Http.json.parseToJsonElement(resp.body) as? JsonObject ?: return emptyList()
        val emails = root["emails"] as? JsonArray ?: return emptyList()
        val result = ArrayList<Email>(emails.size)
        for (msg in emails) {
            val mo = msg as? JsonObject ?: continue
            val message = str(mo, "message")
            val recipient = str(mo, "recipient")
            val bodyText = str(mo, "bodyText")
            val row = mapOf(
                "id" to str(mo, "id"),
                "from" to str(mo, "from"),
                "to" to recipient.ifEmpty { addr },
                "subject" to str(mo, "subject"),
                "text" to bodyText.ifEmpty { message },
                "html" to message,
                "date" to str(mo, "receivedAt"),
            )
            result.add(Normalize.fromMap(row, addr))
        }
        return result
    }

    private fun str(obj: JsonObject, key: String): String =
        (obj[key] as? JsonPrimitive)?.content ?: ""

    companion object {
        private const val BASE_URL = "https://mailmomy.com"
        private const val DEFAULT_DOMAIN = "mailmomy.com"
        private val HEADERS = mapOf(
            "Accept" to "application/json",
            "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
                "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        )
    }
}
