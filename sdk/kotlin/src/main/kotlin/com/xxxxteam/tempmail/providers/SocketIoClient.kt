package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Http
import io.ktor.client.plugins.websocket.*
import io.ktor.client.request.*
import io.ktor.websocket.*
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.serialization.json.*

/**
 * Socket.IO 协议辅助层（Engine.IO v3/v4，WebSocket 传输）。
 *
 * 基于 ktor WebSockets 插件封装 Socket.IO 握手与事件收发，
 * 仅支持短时同步操作（generate / 阻塞式 getEmails）。
 */
internal class SocketIoClient private constructor(
    private val session: DefaultClientWebSocketSession,
) {

    /**
     * 发送 Socket.IO 事件。
     *
     * @param event 事件名
     * @param payloadJson 已序列化的载荷 JSON 文本
     */
    suspend fun emit(event: String, payloadJson: String) {
        session.send(Frame.Text("42[\"$event\",$payloadJson]"))
    }

    /**
     * 等待指定事件名的响应，返回载荷 JSON 文本，超时抛出异常。
     *
     * @param eventName 事件名
     * @param timeoutMs 超时毫秒
     * @return 载荷 JSON 文本
     */
    suspend fun waitForEvent(eventName: String, timeoutMs: Long): String {
        val result = withTimeoutOrNull(timeoutMs) {
            while (true) {
                val frame = session.incoming.receive()
                if (frame !is Frame.Text) continue
                val packet = frame.readText()
                when {
                    packet == "2" -> session.send(Frame.Text("3"))
                    packet.startsWith("42") -> {
                        val parsed = parseEventArray(packet.substring(2))
                        if (parsed != null && parsed.first == eventName) return@withTimeoutOrNull parsed.second
                    }
                }
            }
            @Suppress("UNREACHABLE_CODE") ""
        }
        return result ?: throw RuntimeException("SocketIO: 等待事件 '$eventName' 超时")
    }

    /**
     * 接收下一个 Socket.IO 事件（任意事件名），返回 (事件名, 载荷 JSON)。
     *
     * 自动响应心跳 ping("2")；通道关闭或超时返回 null。
     *
     * @param timeoutMs 超时毫秒
     * @return (事件名, 载荷 JSON) 或 null
     */
    suspend fun receiveEvent(timeoutMs: Long): Pair<String, String>? {
        return withTimeoutOrNull(timeoutMs) {
            try {
                while (true) {
                    val frame = session.incoming.receive()
                    if (frame !is Frame.Text) continue
                    val packet = frame.readText()
                    when {
                        packet == "2" -> session.send(Frame.Text("3"))
                        packet.startsWith("42") -> {
                            val parsed = parseEventArray(packet.substring(2))
                            if (parsed != null) return@withTimeoutOrNull parsed
                        }
                    }
                }
                @Suppress("UNREACHABLE_CODE") null
            } catch (_: Exception) {
                null
            }
        }
    }

    /** 关闭连接。 */
    suspend fun close() {
        try {
            session.close()
        } catch (_: Exception) {
        }
    }

    companion object {
        private const val USER_AGENT =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
                "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"

        private val EIO_VERSIONS = intArrayOf(4, 3)

        /**
         * 连接到 Socket.IO 服务端（自动尝试 EIO v4/v3）。
         *
         * @param host 主机名（如 tempmail.cn）
         * @param timeout 超时秒数
         * @return 已握手的客户端实例
         */
        suspend fun connect(host: String, timeout: Int): SocketIoClient {
            var lastErr: Exception? = null
            for (eio in EIO_VERSIONS) {
                try {
                    val url = "wss://$host/socket.io/?EIO=$eio&transport=websocket"
                    val session = Http.client.webSocketSession(urlString = url) {
                        header("User-Agent", USER_AGENT)
                        header("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8")
                        header("Cache-Control", "no-cache")
                        header("DNT", "1")
                        header("Pragma", "no-cache")
                        header("Origin", "https://$host")
                    }
                    // 等待 Engine.IO open 包（以 "0" 开头）
                    val packet = receiveText(session, timeout * 1000L)
                    if (packet == null || !packet.startsWith("0")) {
                        session.close()
                        throw RuntimeException("SocketIO: EIO=$eio open 包异常")
                    }
                    // 发送 Socket.IO connect
                    session.send(Frame.Text("40"))
                    return SocketIoClient(session)
                } catch (e: Exception) {
                    lastErr = e
                }
            }
            throw RuntimeException("SocketIO: 握手失败: ${lastErr?.message ?: "unknown"}", lastErr)
        }

        /** 接收下一个文本帧，超时返回 null。 */
        private suspend fun receiveText(session: DefaultClientWebSocketSession, timeoutMs: Long): String? =
            withTimeoutOrNull(timeoutMs) {
                while (true) {
                    val frame = session.incoming.receive()
                    if (frame is Frame.Text) return@withTimeoutOrNull frame.readText()
                }
                @Suppress("UNREACHABLE_CODE") null
            }

        /**
         * 解析 Socket.IO 事件数组 ["eventName", payload]，返回 (eventName, payloadJson)。
         *
         * @param json 事件数组 JSON 文本
         * @return (事件名, 载荷 JSON) 或 null
         */
        private fun parseEventArray(json: String): Pair<String, String>? {
            return try {
                val el = Http.json.parseToJsonElement(json)
                val arr = el as? JsonArray ?: return null
                val first = arr.getOrNull(0) as? JsonPrimitive ?: return null
                if (!first.isString) return null
                val payload = if (arr.size > 1) arr[1].toString() else "null"
                first.content to payload
            } catch (_: Exception) {
                null
            }
        }

        /**
         * 从载荷 JSON 文本中提取纯字符串值。
         *
         * @param payloadJson 载荷 JSON 文本
         * @return 纯字符串，非字符串或解析失败返回空串
         */
        fun jsonStringValue(payloadJson: String): String {
            return try {
                val el = Http.json.parseToJsonElement(payloadJson)
                (el as? JsonPrimitive)?.takeIf { it.isString }?.content ?: ""
            } catch (_: Exception) {
                ""
            }
        }
    }
}
