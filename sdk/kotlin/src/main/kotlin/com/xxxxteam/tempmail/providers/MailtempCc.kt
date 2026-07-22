package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * MailTemp.cc 渠道（mailtemp.cc）。
 *
 * POST /api.php action=inbox 创建邮箱，action=fetch 收信，action=view 取详情。
 * 域名 @neocea.com，用户名存入 EmailInfo.token。
 */
object MailtempCc : Provider {

    private const val BASE = "https://mailtemp.cc"
    private const val DOMAIN = "neocea.com"
    private val HEADERS = mapOf(
        "Content-Type" to "application/x-www-form-urlencoded",
        "Accept" to "*/*",
        "Referer" to "$BASE/",
        "Origin" to BASE,
    )

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        val resp = ProviderUtil.httpPost("$BASE/api.php", "action=inbox",
            "application/x-www-form-urlencoded", HEADERS)
        resp.ensureSuccess()
        val rawText = resp.body.trim()
        // 返回值为 JSON 字符串格式（带引号），如 "vindictiverate"
        val username = if (rawText.startsWith("\"") && rawText.endsWith("\"")) {
            rawText.substring(1, rawText.length - 1)
        } else rawText
        if (username.isEmpty()) throw RuntimeException("mailtemp-cc: 返回的用户名无效")
        return EmailInfo(email = "$username@$DOMAIN", channel = "mailtemp-cc", token = username)
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val token = info.token
        if (token.isBlank()) throw RuntimeException("mailtemp-cc: token 为空")
        val addr = info.email.trim().ifEmpty { "$token@$DOMAIN" }
        val resp = ProviderUtil.httpPost("$BASE/api.php",
            "action=fetch&inbox=${ProviderUtil.urlEncode(token)}&last_id=0",
            "application/x-www-form-urlencoded", HEADERS)
        resp.ensureSuccess()
        val arr = Http.json.parseToJsonElement(resp.body) as? JsonArray ?: return emptyList()

        val out = mutableListOf<Email>()
        for (msg in arr.mapNotNull { it as? JsonObject }) {
            val mailId = jsonStr(msg, "id")
            if (mailId.isEmpty()) continue
            val detail = fetchDetail(mailId, token)
            val flat = mapOf(
                "id" to mailId,
                "from" to ProviderUtil.firstNonEmpty(jsonStr(msg, "sender_email"), jsonStr(msg, "sender")),
                "to" to addr,
                "subject" to ProviderUtil.firstNonEmpty(
                    jsonStr(msg, "subject"), detail?.let { jsonStr(it, "subject") } ?: ""),
                "html" to (detail?.let { jsonStr(it, "body_html") } ?: ""),
                "date" to ProviderUtil.firstNonEmpty(
                    jsonStr(msg, "received_at"), detail?.let { jsonStr(it, "received_at") } ?: ""),
            )
            out.add(Normalize.fromMap(flat, addr))
        }
        return out
    }

    /** 拉取单封邮件详情。 */
    private suspend fun fetchDetail(mailId: String, inbox: String): JsonObject? {
        val resp = ProviderUtil.httpPost("$BASE/api.php",
            "action=view&id=${ProviderUtil.urlEncode(mailId)}&inbox=${ProviderUtil.urlEncode(inbox)}",
            "application/x-www-form-urlencoded", HEADERS)
        return if (resp.isOk) Http.json.parseToJsonElement(resp.body) as? JsonObject else null
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
