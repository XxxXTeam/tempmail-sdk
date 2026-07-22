package com.xxxxteam.tempmail

import io.ktor.client.request.*
import io.ktor.http.*
import kotlinx.coroutines.*
import kotlinx.serialization.json.*

/**
 * 匿名遥测上报
 *
 * 默认开启，可通过环境变量 TEMPMAIL_TELEMETRY_ENABLED=false 关闭。
 * 上报内容仅包含渠道使用频次统计，不包含任何邮件内容或用户信息。
 */
object Telemetry {
    private const val DEFAULT_URL = "https://telemetry.tempmail.sdk/v1/event"
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    /** 上报一次渠道使用事件 */
    fun reportUsage(channel: String, action: String) {
        // 热路径使用零拷贝读取，仅做只读判断
        val cfg = Config.current()
        if (cfg.telemetryEnabled == false) return

        val url = cfg.telemetryUrl ?: DEFAULT_URL
        scope.launch {
            try {
                Http.client.post(url) {
                    contentType(ContentType.Application.Json)
                    setBody(buildJsonObject {
                        put("sdk", "kotlin")
                        put("version", "0.1.0")
                        put("channel", channel)
                        put("action", action)
                        put("ts", System.currentTimeMillis())
                    }.toString())
                }
            } catch (_: Exception) {
                // 遥测失败不影响主流程
            }
        }
    }
}
