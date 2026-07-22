package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import com.xxxxteam.tempmail.providers.ProviderUtil.firstNonEmpty
import com.xxxxteam.tempmail.providers.ProviderUtil.str
import kotlinx.serialization.json.*

/**
 * Mailinator 及其姊妹域名渠道（mailinator.com，domain=public 公共收件箱）。
 *
 * 参数化构造，被约 40 个姊妹域名渠道复用。本地生成随机地址，
 * 通过 /api/v2/domains/public 接口逐封拉取纯文本与 HTML 正文。
 *
 * @property channel 渠道标识
 * @property domain 邮箱域名
 */
class Mailinator(
    private val channel: String,
    private val domain: String,
) : Provider {

    /** 创建邮箱（本地随机生成，无需 API 调用）。 */
    override suspend fun generate(): EmailInfo {
        val email = "${ProviderUtil.randomString(12)}@$domain"
        return EmailInfo(email = email, channel = channel)
    }

    /** 获取邮件列表，逐封拉取纯文本与 HTML 正文。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        var inbox = info.email.trim()
        val at = inbox.indexOf('@')
        if (at >= 0) inbox = inbox.substring(0, at)
        if (inbox.isEmpty()) return emptyList()

        val listData = requestJson("$BASE_URL/api/v2/domains/$PUBLIC_DOMAIN/inboxes/$inbox")
        val messages = parseMessages(listData)
        val result = ArrayList<Email>()
        for (msg in messages) {
            val id = firstNonEmpty(str(msg, "id"), str(msg, "messageId"))
            var textPayload: JsonObject? = null
            var htmlPayload: JsonObject? = null
            if (id.isNotEmpty()) {
                textPayload = requestJson("$BASE_URL/api/v2/domains/$PUBLIC_DOMAIN/messages/$id/text")
                htmlPayload = requestJson("$BASE_URL/api/v2/domains/$PUBLIC_DOMAIN/messages/$id/texthtml")
            }
            val to = str(msg, "to")
            val flat = mapOf(
                "id" to id,
                "from" to firstNonEmpty(str(msg, "from"), str(msg, "origfrom")),
                "to" to to.ifEmpty { info.email },
                "subject" to str(msg, "subject"),
                "text" to (textPayload?.let { str(it, "text/plain") } ?: ""),
                "html" to (htmlPayload?.let { str(it, "text/html") } ?: ""),
                "date" to firstNonEmpty(str(msg, "time"), str(msg, "date")),
            )
            result.add(Normalize.fromMap(flat, info.email))
        }
        return result
    }

    /** GET 并解析为 JsonObject，失败返回 null。 */
    private suspend fun requestJson(url: String): JsonObject? {
        return try {
            val resp = ProviderUtil.httpGet(url, HEADERS)
            if (!resp.isOk) null else ProviderUtil.parseObject(resp.body)
        } catch (_: Exception) {
            null
        }
    }

    /** 从响应中提取消息列表（兼容 msgs / data 键）。 */
    private fun parseMessages(data: JsonObject?): List<JsonObject> {
        if (data == null) return emptyList()
        for (key in arrayOf("msgs", "data")) {
            val arr = data[key] as? JsonArray ?: continue
            return arr.mapNotNull { it as? JsonObject }
        }
        return emptyList()
    }

    companion object {
        private const val BASE_URL = "https://mailinator.com"
        private const val PUBLIC_DOMAIN = "public"
        private val HEADERS = mapOf(
            "Accept" to "application/json",
            "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
                "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        )
    }
}
