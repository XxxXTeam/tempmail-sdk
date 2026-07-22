package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * tempmail-cn 渠道实现（tempmail.cn）。
 *
 * generate: Socket.IO 连接 → request mailbox → 接收 mailbox 事件；
 * getEmails: REST API GET https://{host}/api/mails/{email}。
 */
object TempmailCn : Provider {

    private const val CHANNEL = "tempmail-cn"
    private const val DEFAULT_HOST = "tempmail.cn"

    private fun baseHeaders(): MutableMap<String, String> = mutableMapOf(
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Cache-Control" to "no-cache",
        "DNT" to "1",
        "Pragma" to "no-cache",
    )

    /** 通过 Socket.IO 请求邮箱地址。 */
    override suspend fun generate(): EmailInfo {
        val host = DEFAULT_HOST
        var sio: SocketIoClient? = null
        try {
            sio = SocketIoClient.connect(host, 15)
            sio.emit("request mailbox", "true")
            val payload = sio.waitForEvent("mailbox", 15000)
            val mailbox = SocketIoClient.jsonStringValue(payload).trim()
            if (mailbox.isEmpty()) throw RuntimeException("$CHANNEL: mailbox 为空")
            return EmailInfo(email = "$mailbox@$host", channel = CHANNEL)
        } catch (e: RuntimeException) {
            throw e
        } catch (e: Exception) {
            throw RuntimeException("$CHANNEL: generate 失败: ${e.message}", e)
        } finally {
            sio?.close()
        }
    }

    /** 通过 REST API 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val trimmed = info.email.trim()
        val atPos = trimmed.indexOf('@')
        if (atPos <= 0) throw IllegalArgumentException("$CHANNEL: invalid email address")
        var host = trimmed.substring(atPos + 1)
        if (host.isEmpty()) host = DEFAULT_HOST

        val url = "https://$host/api/mails/${ProviderUtil.urlEncode(trimmed)}"
        val headers = baseHeaders().apply {
            put("Accept", "application/json")
            put("Referer", "https://$host/")
        }
        val resp = ProviderUtil.httpGet(url, headers)
        if (resp.statusCode >= 400) return emptyList()
        val data = ProviderUtil.parseObject(resp.body) ?: return emptyList()
        val mails = ProviderUtil.arr(data, "mails") ?: return emptyList()
        val result = mutableListOf<Email>()
        for (item in mails.filterIsInstance<JsonObject>()) {
            val flat = flattenMail(item, trimmed)
            val em = Normalize.fromMap(flat, trimmed)
            if (em.id.isNotEmpty()) result.add(em)
        }
        return result
    }

    /** 展平邮件原始数据。 */
    private fun flattenMail(raw: JsonObject, recipient: String): Map<String, Any?> {
        val headers = raw["headers"] as? JsonObject
        var msgId = ProviderUtil.firstNonEmpty(
            ProviderUtil.str(raw, "id"),
            ProviderUtil.str(raw, "messageId"),
            ProviderUtil.str(headers, "message-id"),
            ProviderUtil.str(headers, "messageId"),
        )
        if (msgId.isEmpty()) {
            msgId = listOf(
                ProviderUtil.str(headers, "from"),
                ProviderUtil.str(headers, "subject"),
                ProviderUtil.str(headers, "date"),
                recipient,
            ).joinToString("\n")
        }
        return mapOf(
            "id" to msgId,
            "from" to ProviderUtil.firstNonEmpty(ProviderUtil.str(headers, "from"), ProviderUtil.str(raw, "from")),
            "to" to recipient,
            "subject" to ProviderUtil.firstNonEmpty(ProviderUtil.str(headers, "subject"), ProviderUtil.str(raw, "subject")),
            "text" to ProviderUtil.str(raw, "text"),
            "html" to ProviderUtil.str(raw, "html"),
            "date" to ProviderUtil.firstNonEmpty(ProviderUtil.str(headers, "date"), ProviderUtil.str(raw, "date")),
        )
    }
}
