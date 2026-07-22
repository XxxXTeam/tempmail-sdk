package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Http
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * maildrop.cc 渠道（maildrop.cc）。
 *
 * GraphQL API，无认证。本地随机生成用户名 + @maildrop.cc，先查 inbox 列表再逐封查详情。
 */
object MaildropCc : Provider {

    private const val DOMAIN = "maildrop.cc"
    private const val GRAPHQL_URL = "https://api.maildrop.cc/graphql"
    private val HEADERS = mapOf(
        "Accept" to "application/json",
        "Content-Type" to "application/json",
        "Origin" to "https://maildrop.cc",
        "Referer" to "https://maildrop.cc/",
        "User-Agent" to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    )

    /** 创建临时邮箱（本地生成，无需 API 调用）。 */
    override suspend fun generate(): EmailInfo {
        val email = "${ProviderUtil.randomString(10)}@$DOMAIN"
        return EmailInfo(email = email, channel = "maildrop-cc")
    }

    /** 获取邮件列表。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val mb = mailbox(info.email)
        if (mb.isBlank()) throw RuntimeException("maildrop-cc: 邮箱地址为空")
        val mbJson = Json.encodeToString(JsonPrimitive.serializer(), JsonPrimitive(mb))
        val inboxResp = doGraphql("query { inbox(mailbox: $mbJson) { id headerfrom subject date } }")
        val inboxArr = (inboxResp["data"] as? JsonObject)?.get("inbox") as? JsonArray ?: return emptyList()

        val out = mutableListOf<Email>()
        for (item in inboxArr.mapNotNull { it as? JsonObject }) {
            val mid = jsonStr(item, "id")
            val flat = mutableMapOf<String, Any?>(
                "id" to mid,
                "from" to jsonStr(item, "headerfrom"),
                "subject" to jsonStr(item, "subject"),
                "date" to jsonStr(item, "date"),
            )
            if (mid.isNotEmpty()) {
                runCatching {
                    val midJson = Json.encodeToString(JsonPrimitive.serializer(), JsonPrimitive(mid))
                    val msgResp = doGraphql(
                        "query { message(mailbox: $mbJson, id: $midJson) { id headerfrom subject date html } }")
                    val msg = (msgResp["data"] as? JsonObject)?.get("message") as? JsonObject
                    if (msg != null) {
                        flat["id"] = jsonStr(msg, "id").ifEmpty { mid }
                        flat["from"] = jsonStr(msg, "headerfrom")
                        flat["subject"] = jsonStr(msg, "subject")
                        flat["date"] = jsonStr(msg, "date")
                        flat["html"] = jsonStr(msg, "html")
                    }
                }
            }
            out.add(Normalize.fromMap(flat, info.email))
        }
        return out
    }

    /** 发起 GraphQL 请求。 */
    private suspend fun doGraphql(query: String): JsonObject {
        val body = buildJsonObject { put("query", query) }.toString()
        val resp = ProviderUtil.httpPost(GRAPHQL_URL, body, "application/json", HEADERS)
        resp.ensureSuccess()
        return Http.json.parseToJsonElement(resp.body) as? JsonObject ?: JsonObject(emptyMap())
    }

    /** 提取邮箱用户名。 */
    private fun mailbox(email: String): String {
        if (email.isBlank()) return ""
        return if (email.contains("@")) email.substringBefore("@") else email
    }
}

/** 从 JSON 对象读取字符串字段，缺失返回空串（文件私有）。 */
private fun jsonStr(obj: JsonObject, key: String): String =
    (obj[key] as? JsonPrimitive)?.contentOrNull ?: ""
