package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * Mailticking 渠道实现（www.mailticking.com，旧域名 temporary-mail.net 的更名站）。
 *
 * 实测协议（三类请求均以 application/json 交流，站点在 Cloudflare 后）：
 *   1. 建箱 POST /get-mailbox body {"types":["4"]}（4=独立域名，排除 Gmail 别名）
 *      响应 {"success":true,"email":"xxx@domain","activate_token":"..."}
 *   2. 激活 POST /activate-email body {"email":..,"source":"api","activate_token":..}
 *      响应 {"success":true}
 *   3. 列信 POST /get-emails?lang=en body {"email":..,"code":..}
 *      空箱实测响应 {"emails":[],"success":true}；空闲超时/被改绑后返回
 *      {"success":false,"needNewEmail":true,...}，此时返回错误提示换箱。
 *
 * 读信正文无公开端点：GetEmails 只具备列表能力，列表字段名采用多候选提取
 * 策略，命中多少映射多少（Normalize 既有候选字段负责兜底）。
 */
object Mailticking : Provider {

    private const val CHANNEL = "mailticking"
    private const val BASE_URL = "https://www.mailticking.com"

    private val headers = mapOf(
        "Accept" to "application/json",
        "Accept-Language" to "en-US,en;q=0.9",
        "Content-Type" to "application/json",
        "Referer" to "$BASE_URL/",
        "Origin" to BASE_URL,
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36",
    )

    /** 执行 JSON POST 请求并解析响应；响应非 2xx 或 success=false 时抛错。 */
    private suspend fun doPost(path: String, payload: JsonObject): JsonObject {
        val resp = ProviderUtil.httpPost("$BASE_URL$path", payload.toString(), "application/json", headers)
        val data = ProviderUtil.parseObject(resp.body)
        if (!resp.isOk) {
            val msg = ProviderUtil.str(data, "error").ifEmpty { ProviderUtil.str(data, "message").ifEmpty { resp.body } }
            throw RuntimeException("mailticking: http ${resp.statusCode}: $msg")
        }
        val ok = (data?.get("success") as? JsonPrimitive)?.booleanOrNull ?: false
        if (!ok) {
            val msg = ProviderUtil.str(data, "error").ifEmpty { ProviderUtil.str(data, "message").ifEmpty { "unknown error" } }
            throw RuntimeException("mailticking: 请求失败: $msg")
        }
        return data
    }

    /**
     * 创建 mailticking 邮箱账号。
     * 请求 type=4（独立域名）固定取独立域名邮箱，建箱后立即激活会话；
     * token 必须携带 activate_token（列信协议依赖激活会话），不能为空。
     */
    override suspend fun generate(): EmailInfo {
        val box = doPost(
            "/get-mailbox",
            buildJsonObject { putJsonArray("types") { add("4") } },
        )
        var token = ProviderUtil.str(box, "code").trim()
        if (token.isEmpty()) token = ProviderUtil.str(box, "email").trim()
        val email = ProviderUtil.str(box, "email").trim()
        if (token.isEmpty() || email.isEmpty()) {
            throw RuntimeException("mailticking: get-mailbox 返回空 email/activate_token")
        }

        // 激活邮箱，使后续列信请求与服务器记录的最新会话一致
        doPost(
            "/activate-email",
            buildJsonObject {
                put("email", email)
                put("source", "api")
                put("activate_token", token)
            },
        )

        return EmailInfo(email = email, channel = CHANNEL, token = token)
    }

    /**
     * 获取 mailticking 邮箱的邮件列表。
     * 空箱返回空列表不报错；需要换箱的响应（needNewEmail）返回语义错误。
     */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val addr = info.email.trim()
        val tk = info.token.trim()
        if (addr.isEmpty()) throw RuntimeException("mailticking: 邮箱地址为空")
        if (tk.isEmpty()) throw RuntimeException("mailticking: activate code 为空")

        val resp = ProviderUtil.httpPost(
            "$BASE_URL/get-emails?lang=en",
            buildJsonObject {
                put("email", addr)
                put("code", tk)
            }.toString(),
            "application/json",
            headers,
        )
        val data = ProviderUtil.parseObject(resp.body)
        if (!resp.isOk) {
            val msg = ProviderUtil.str(data, "error").ifEmpty { ProviderUtil.str(data, "message").ifEmpty { resp.body } }
            throw RuntimeException("mailticking: http ${resp.statusCode}: $msg")
        }
        val ok = (data?.get("success") as? JsonPrimitive)?.booleanOrNull ?: false
        if (!ok) {
            val needNew = (data?.get("needNewEmail") as? JsonPrimitive)?.booleanOrNull ?: false
            if (needNew) throw RuntimeException("mailticking: 邮箱已过期，请重新建箱")
            throw RuntimeException("mailticking: get-emails 失败")
        }
        val emails = ProviderUtil.arr(data, "emails") ?: return emptyList()

        return emails.filterIsInstance<JsonObject>().map { raw ->
            Normalize.fromJson(JsonObject(migratedFields(raw)), addr)
        }
    }

    /**
     * 尽量映射列表字段到统一字段名。
     * 官网首页表格只有 SENDER/SUBJECT/TIME 三列，字段全名缺少可观测证据，
     * 因此对常见字段做多候选提取；未命中的字段留给 Normalize 自己处理。
     */
    private fun migratedFields(raw: JsonObject): Map<String, JsonElement> {
        val m = raw.toMutableMap()
        if (!m.containsKey("from")) {
            for (key in listOf(
                "mail_from", "from_mail", "from_email", "sender_address", "from_address",
                "send_addr", "mail_addr", "address_from", "ho_from", "fromname", "fromS",
            )) {
                val v = m[key] ?: continue
                if (v !is JsonNull && v is JsonPrimitive && v.content.isNotBlank()) {
                    m["from"] = v
                    break
                }
            }
        }
        if (!m.containsKey("sender")) {
            val from = m["from"] as? JsonPrimitive
            if (from != null && from.content.isNotBlank()) m["sender"] = from
        }
        if (!m.containsKey("id")) m["mail_id"]?.let { m["id"] = it }
        if (!m.containsKey("date")) m["received_at"]?.let { m["date"] = it }
        return m
    }
}