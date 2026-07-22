package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import kotlinx.serialization.json.*

/**
 * Socket.IO 临时邮箱共享实现。
 *
 * 用于 mjj.cm、linshi.co 等使用相同 Socket.IO 协议的站点：
 * generate: 连接 → request shortid → 接收 shortid → 断开；
 * getEmails: 连接 → set shortid → 阻塞监听 mail 事件 → 收集后断开。
 */
internal object SocketIoMail {

    /** 收信阻塞等待时间（毫秒）。 */
    private const val GET_EMAILS_WAIT_MS = 5000L

    /**
     * 请求 shortid 并生成邮箱。
     *
     * @param channel 渠道标识
     * @param host 主机名
     * @return 邮箱信息
     */
    suspend fun generate(channel: String, host: String): EmailInfo {
        var sio: SocketIoClient? = null
        try {
            sio = SocketIoClient.connect(host, 15)
            sio.emit("request shortid", "true")
            val payload = sio.waitForEvent("shortid", 15000)
            val shortid = SocketIoClient.jsonStringValue(payload).trim()
            if (shortid.isEmpty()) throw RuntimeException("$channel: shortid 为空")
            return EmailInfo(email = "$shortid@$host", channel = channel)
        } catch (e: RuntimeException) {
            throw e
        } catch (e: Exception) {
            throw RuntimeException("$channel: generate 失败: ${e.message}", e)
        } finally {
            sio?.close()
        }
    }

    /**
     * 阻塞式收信：连接后订阅 shortid，等待 mail 事件。
     *
     * @param channel 渠道标识
     * @param email 邮箱地址
     * @return 邮件列表
     */
    suspend fun getEmails(channel: String, email: String): List<Email> {
        val trimmed = email.trim()
        val atPos = trimmed.indexOf('@')
        if (atPos <= 0) throw IllegalArgumentException("$channel: invalid email address")
        val local = trimmed.substring(0, atPos)
        var host = trimmed.substring(atPos + 1)
        if (host.isEmpty()) host = channel

        var sio: SocketIoClient? = null
        try {
            sio = SocketIoClient.connect(host, 15)
            sio.emit("set shortid", "\"$local\"")

            val result = mutableListOf<Email>()
            val deadline = System.currentTimeMillis() + GET_EMAILS_WAIT_MS
            while (true) {
                val remaining = deadline - System.currentTimeMillis()
                if (remaining <= 0) break
                val event = sio.receiveEvent(remaining) ?: break
                if (event.first == "mail") {
                    val payload = ProviderUtil.parse(event.second)
                    if (payload is JsonObject) {
                        result.add(Normalize.fromMap(flattenMail(payload, trimmed), trimmed))
                    }
                }
            }
            return result
        } catch (e: RuntimeException) {
            throw e
        } catch (e: Exception) {
            throw RuntimeException("$channel: getEmails 失败: ${e.message}", e)
        } finally {
            sio?.close()
        }
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
                ProviderUtil.firstNonEmpty(ProviderUtil.str(headers, "from"), ProviderUtil.str(raw, "from")),
                ProviderUtil.firstNonEmpty(ProviderUtil.str(headers, "subject"), ProviderUtil.str(raw, "subject")),
                ProviderUtil.firstNonEmpty(ProviderUtil.str(headers, "date"), ProviderUtil.str(raw, "date")),
                recipient,
            ).joinToString("\n")
        }
        return mapOf(
            "id" to msgId,
            "from" to ProviderUtil.firstNonEmpty(ProviderUtil.str(headers, "from"), ProviderUtil.str(raw, "from")),
            "to" to recipient,
            "subject" to ProviderUtil.firstNonEmpty(ProviderUtil.str(headers, "subject"), ProviderUtil.str(raw, "subject")),
            "text" to ProviderUtil.firstNonEmpty(ProviderUtil.str(raw, "text"), ProviderUtil.str(raw, "body")),
            "html" to ProviderUtil.str(raw, "html"),
            "date" to ProviderUtil.firstNonEmpty(ProviderUtil.str(headers, "date"), ProviderUtil.str(raw, "date")),
        )
    }
}
