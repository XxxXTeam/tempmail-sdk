package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*
import java.nio.charset.StandardCharsets
import java.util.Base64
import kotlin.random.Random

/**
 * TempGBox 渠道（tempgbox.net/api/proxy）。
 *
 * JSON API，响应以 base64 编码嵌入 HTML data-x 属性中。token 为 device id。
 */
object Tempgbox : Provider {

    private const val CHANNEL = "tempgbox"
    private const val API_URL = "https://tempgbox.net/api/proxy"
    private const val ORIGIN = "https://tempgbox.net"

    private fun defaultHeaders(deviceId: String): Map<String, String> {
        val ip = randomIp()
        return mapOf(
            "Accept" to "text/html,application/json",
            "Content-Type" to "application/json",
            "Origin" to ORIGIN,
            "Referer" to "$ORIGIN/",
            "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
                "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
            "X-Device-ID" to deviceId,
            "X-Forwarded-For" to ip,
            "X-Real-IP" to ip,
            "X-Originating-IP" to ip,
        )
    }

    private fun randomDeviceId(): String {
        val bytes = ByteArray(32)
        Random.nextBytes(bytes)
        return bytes.joinToString("") { "%02x".format(it.toInt() and 0xFF) }
    }

    private fun randomIp(): String =
        "${Random.nextInt(1, 255)}.${Random.nextInt(1, 255)}.${Random.nextInt(1, 255)}.${Random.nextInt(1, 255)}"

    /** 从 HTML 的 data-x 属性中解码 base64 payload。 */
    private fun decodePayload(html: String): JsonObject {
        var marker = "data-x=\""
        var start = html.indexOf(marker)
        var quote = "\""
        if (start < 0) {
            marker = "data-x='"
            start = html.indexOf(marker)
            quote = "'"
        }
        if (start < 0) throw RuntimeException("tempgbox: 缺少编码响应 payload")
        start += marker.length
        val end = html.indexOf(quote, start)
        if (end < 0) throw RuntimeException("tempgbox: 编码响应 payload 格式错误")
        val encoded = html.substring(start, end)
        val decoded = String(Base64.getDecoder().decode(encoded), StandardCharsets.UTF_8)
        return Http.json.parseToJsonElement(decoded) as? JsonObject
            ?: throw RuntimeException("tempgbox: payload 解析失败")
    }

    /** 通过 proxy 发起 POST 请求。 */
    private suspend fun postProxy(route: String, deviceId: String, payload: JsonObject): JsonObject {
        val url = "$API_URL?route=${ProviderUtil.urlEncode(route)}"
        val resp = ProviderUtil.httpPost(url, payload.toString(), "application/json", defaultHeaders(deviceId))
        val data = decodePayload(resp.body)
        if (!resp.isOk) {
            val reason = ProviderUtil.firstNonEmpty(
                jsonStr(data, "detail"), jsonStr(data, "error"), jsonStr(data, "message"),
            )
            throw RuntimeException("tempgbox $route failed: ${resp.statusCode} $reason")
        }
        return data
    }

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val deviceId = randomDeviceId()
        val payload = buildJsonObject { put("variant", "googlemail") }
        val data = postProxy("generate", deviceId, payload)
        val alias = data["alias"] as? JsonObject
            ?: throw RuntimeException("tempgbox: 响应中缺少 alias")
        val email = ProviderUtil.firstNonEmpty(jsonStr(alias, "email"), jsonStr(alias, "alias"))
        if (email.isEmpty()) throw RuntimeException("tempgbox: 缺少邮箱地址")
        val createdAt = jsonStr(alias, "created_at")
        return EmailInfo(email = email, channel = CHANNEL, token = deviceId, createdAt = createdAt)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token
        if (token.isBlank()) throw RuntimeException("tempgbox: 缺少 device id")
        val email = info.email
        val payload = buildJsonObject { put("email", email) }
        val data = postProxy("inbox", token, payload)
        val msgs = data["messages"] as? JsonArray ?: return emptyList()
        return msgs.mapNotNull { it as? JsonObject }.map { raw ->
            val flat = mapOf(
                "id" to ProviderUtil.firstNonEmpty(
                    jsonStr(raw, "messageId"), jsonStr(raw, "message_id"), jsonStr(raw, "id"),
                ),
                "from" to ProviderUtil.firstNonEmpty(jsonStr(raw, "from"), jsonStr(raw, "sender")),
                "to" to email,
                "subject" to jsonStr(raw, "subject"),
                "body" to ProviderUtil.firstNonEmpty(
                    jsonStr(raw, "text"), jsonStr(raw, "body_text"),
                    jsonStr(raw, "html"), jsonStr(raw, "body_html"),
                ),
                "date" to ProviderUtil.firstNonEmpty(jsonStr(raw, "date"), jsonStr(raw, "received_at")),
            )
            Normalize.fromMap(flat, email)
        }
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
