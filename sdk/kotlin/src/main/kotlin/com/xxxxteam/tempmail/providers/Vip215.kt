package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import io.ktor.client.plugins.websocket.*
import io.ktor.client.request.*
import io.ktor.websocket.*
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.serialization.json.*

/**
 * vip-215 渠道（vip.215.im）。
 *
 * generate: GET 首页获取 cookie → POST /api/temp-inbox 创建收件箱（token 为 JWT）；
 * getEmails: GET /v1/auth/ws-ticket 获取 ticket → WebSocket /v1/ws?token={ticket}
 * 监听 message.new 事件（阻塞 5 秒）。
 */
object Vip215 : Provider {

    private const val CHANNEL = "vip-215"
    private const val BASE = "https://vip.215.im"
    private const val USER_AGENT =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36 Edg/148.0.0.0"

    /** 收信阻塞等待时间（毫秒）。 */
    private const val GET_EMAILS_WAIT_MS = 5000L

    private const val SYNTHETIC_MARKER = "【tempmail-sdk|synthetic|vip-215|v1】"

    private fun apiHeaders(): Map<String, String> = mapOf(
        "User-Agent" to USER_AGENT,
        "Accept" to "*/*",
        "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        "Cache-Control" to "no-cache",
        "Content-Type" to "application/json",
        "DNT" to "1",
        "Origin" to BASE,
        "Pragma" to "no-cache",
        "Referer" to "$BASE/",
        "Sec-CH-UA" to "\"Chromium\";v=\"148\", \"Microsoft Edge\";v=\"148\", \"Not/A)Brand\";v=\"99\"",
        "Sec-CH-UA-Mobile" to "?0",
        "Sec-CH-UA-Platform" to "\"Windows\"",
        "Sec-Fetch-Dest" to "empty",
        "Sec-Fetch-Mode" to "cors",
        "Sec-Fetch-Site" to "same-origin",
        "X-Locale" to "zh",
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val cookie = establishSession()
        val headers = apiHeaders() + ("Cookie" to cookie)
        val resp = ProviderUtil.httpPost("$BASE/api/temp-inbox", "", "application/json", headers)
        if (resp.statusCode >= 400) throw RuntimeException("$CHANNEL: 创建收件箱失败: HTTP ${resp.statusCode}")
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject
        if (body == null || !jsonBool(body, "success")) throw RuntimeException("$CHANNEL: success=false")
        val data = body["data"] as? JsonObject ?: throw RuntimeException("$CHANNEL: 缺少 data")
        val address = jsonStr(data, "address")
        val token = jsonStr(data, "token")
        if (address.isEmpty() || token.isEmpty()) throw RuntimeException("$CHANNEL: 缺少 address 或 token")
        return EmailInfo(email = address, channel = CHANNEL, token = token, createdAt = jsonStr(data, "createdAt"))
    }

    /** WebSocket 阻塞式收信：连接后监听 message.new 事件。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token
        val email = info.email
        if (token.isEmpty()) throw RuntimeException("$CHANNEL: token 不能为空")

        val ticket = fetchWsTicket(token)
        val wsUrl = "wss://vip.215.im/v1/ws?token=${ProviderUtil.urlEncode(ticket)}"
        val result = mutableListOf<Email>()
        val session = Http.client.webSocketSession(urlString = wsUrl) {
            header("User-Agent", USER_AGENT)
            header("Origin", BASE)
        }
        try {
            val deadline = System.currentTimeMillis() + GET_EMAILS_WAIT_MS
            while (true) {
                val remaining = deadline - System.currentTimeMillis()
                if (remaining <= 0) break
                val msg = withTimeoutOrNull(remaining) {
                    while (true) {
                        val frame = session.incoming.receive()
                        if (frame is Frame.Text) return@withTimeoutOrNull frame.readText()
                    }
                    @Suppress("UNREACHABLE_CODE") null
                } ?: break

                val parsed = Http.json.parseToJsonElement(msg) as? JsonObject ?: continue
                if (jsonStr(parsed, "type") != "message.new") continue
                val data = parsed["data"] as? JsonObject ?: continue

                val (text, html) = buildSyntheticBodies(email, data)
                val flat = mapOf(
                    "id" to jsonStr(data, "id"),
                    "from" to jsonStr(data, "from"),
                    "subject" to jsonStr(data, "subject"),
                    "date" to jsonStr(data, "date"),
                    "to" to email,
                    "body" to ProviderUtil.firstNonEmpty(text, html),
                )
                val em = Normalize.fromMap(flat, email)
                if (em.id.isNotEmpty()) result.add(em)
            }
            return result
        } finally {
            try {
                session.close()
            } catch (_: Exception) {
            }
        }
    }

    /** 获取首页 cookie（yyds_homepage_bridge + yyds_homepage_device）。 */
    private suspend fun establishSession(): String {
        val h = mapOf(
            "User-Agent" to USER_AGENT,
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
            "Accept-Language" to "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
            "Cache-Control" to "no-cache",
            "DNT" to "1",
            "Pragma" to "no-cache",
        )
        val resp = ProviderUtil.httpGet("$BASE/", h)
        if (resp.statusCode >= 400) throw RuntimeException("$CHANNEL: 首页请求失败")
        val parts = resp.setCookies.mapNotNull { sc ->
            val semi = sc.indexOf(';')
            val nv = if (semi > 0) sc.substring(0, semi) else sc
            if (nv.contains("=")) nv.trim() else null
        }
        val cookie = parts.joinToString("; ")
        if (!cookie.contains("yyds_homepage_bridge=") || !cookie.contains("yyds_homepage_device=")) {
            throw RuntimeException("$CHANNEL: 缺少首页 cookie")
        }
        return cookie
    }

    /** 获取 WebSocket ticket。 */
    private suspend fun fetchWsTicket(jwt: String): String {
        val headers = apiHeaders() + ("Authorization" to "Bearer $jwt")
        val resp = ProviderUtil.httpGet("$BASE/v1/auth/ws-ticket", headers)
        if (resp.statusCode >= 400) throw RuntimeException("$CHANNEL: ws-ticket 请求失败")
        val body = Http.json.parseToJsonElement(resp.body) as? JsonObject
        if (body == null || !jsonBool(body, "success")) throw RuntimeException("$CHANNEL: ws-ticket success=false")
        val ticket = (body["data"] as? JsonObject)?.let { jsonStr(it, "ticket") } ?: ""
        if (ticket.isEmpty()) throw RuntimeException("$CHANNEL: 缺少 ws ticket")
        return ticket
    }

    /** 构建合成正文（WebSocket 推送无完整 body，用元数据摘要填充），返回 (text, html)。 */
    private fun buildSyntheticBodies(recipientEmail: String, data: JsonObject): Pair<String, String> {
        val id = sanitize(jsonStr(data, "id"))
        val subject = sanitize(jsonStr(data, "subject"))
        val from = sanitize(jsonStr(data, "from"))
        val to = sanitize(recipientEmail)
        val date = sanitize(jsonStr(data, "date"))

        val text = SYNTHETIC_MARKER + "\n" +
            "id: $id\n" +
            "subject: $subject\n" +
            "from: $from\n" +
            "to: $to\n" +
            "date: $date"

        val html = "<div class=\"tempmail-sdk-synthetic\" data-tempmail-sdk-format=\"synthetic-v1\" " +
            "data-channel=\"vip-215\"><dl class=\"tempmail-sdk-meta\">" +
            "<dt>id</dt><dd>${escHtml(id)}</dd>" +
            "<dt>subject</dt><dd>${escHtml(subject)}</dd>" +
            "<dt>from</dt><dd>${escHtml(from)}</dd>" +
            "<dt>to</dt><dd>${escHtml(to)}</dd>" +
            "<dt>date</dt><dd>${escHtml(date)}</dd>" +
            "</dl></div>"

        return text to html
    }

    private fun sanitize(v: String): String =
        v.replace("\r\n", " ").replace("\r", " ").replace("\n", " ").trim()

    private fun escHtml(s: String): String =
        s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;")
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""

/** 从 JSON 对象读取布尔字段（文件私有）。 */
private fun jsonBool(obj: JsonObject, key: String): Boolean =
    (obj[key] as? JsonPrimitive)?.booleanOrNull ?: false
