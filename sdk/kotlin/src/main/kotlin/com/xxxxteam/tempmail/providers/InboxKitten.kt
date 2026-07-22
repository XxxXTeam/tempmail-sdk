package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Email
import com.xxxxteam.tempmail.EmailInfo
import com.xxxxteam.tempmail.Normalize
import com.xxxxteam.tempmail.Provider
import kotlinx.serialization.json.*

/**
 * InboxKitten 渠道（inboxkitten.com/api/v1/mail）。
 */
object InboxKitten : Provider {

    private const val CHANNEL = "inboxkitten"
    private const val API_BASE = "https://inboxkitten.com/api/v1/mail"
    private const val DOMAIN = "inboxkitten.com"

    private val headersJson = mapOf("Accept" to "application/json")
    private val headersHtml = mapOf("Accept" to "text/html,*/*")
    private val tagRe = Regex("<[^>]+>", RegexOption.DOT_MATCHES_ALL)

    /** 创建临时邮箱。 */
    override suspend fun generate(): EmailInfo {
        return EmailInfo(email = "${ProviderUtil.randomLocalSdk()}@$DOMAIN", channel = CHANNEL)
    }

    /** 获取邮件列表，逐封拉取正文。 */
    override suspend fun getEmails(info: EmailInfo): List<Email> {
        val local = info.email.trim().substringBefore("@")
        if (local.isEmpty()) throw RuntimeException("inboxkitten: empty email")
        val resp = ProviderUtil.httpGet("$API_BASE/list?recipient=${ProviderUtil.urlEncode(local)}", headersJson)
        resp.ensureSuccess()
        val arr = ProviderUtil.parse(resp.body) as? JsonArray ?: return emptyList()
        return arr.filterIsInstance<JsonObject>().map { Normalize.fromMap(detailRaw(it, info.email), info.email) }
    }

    private fun objStr(o: JsonObject?, key: String): String = ProviderUtil.str(o, key)

    private suspend fun detailRaw(row: JsonObject, recipient: String): Map<String, Any?> {
        val storage = row["storage"] as? JsonObject
        val region = objStr(storage, "region").trim()
        val key = objStr(storage, "key").trim()
        val message = row["message"] as? JsonObject
        val msgHeaders = message?.get("headers") as? JsonObject
        val envelope = row["envelope"] as? JsonObject

        val raw = linkedMapOf<String, Any?>(
            "id" to key,
            "messageId" to key,
            "from" to ProviderUtil.firstNonEmpty(objStr(msgHeaders, "from"), objStr(envelope, "sender")),
            "to" to ProviderUtil.firstNonEmpty(objStr(row, "recipient"), recipient),
            "subject" to objStr(msgHeaders, "subject"),
        )
        if (region.isEmpty() || key.isEmpty()) return raw
        try {
            val metaResp = ProviderUtil.httpGet(
                "$API_BASE/getKey?region=${ProviderUtil.urlEncode(region)}&key=${ProviderUtil.urlEncode(key)}",
                headersJson,
            )
            metaResp.ensureSuccess()
            val meta = ProviderUtil.parseObject(metaResp.body)
            val htmlResp = ProviderUtil.httpGet(
                "$API_BASE/getHtml?region=${ProviderUtil.urlEncode(region)}&key=${ProviderUtil.urlEncode(key)}",
                headersHtml,
            )
            htmlResp.ensureSuccess()
            val html = htmlResp.body
            raw["from"] = ProviderUtil.firstNonEmpty(objStr(meta, "name"), raw["from"] as String)
            raw["subject"] = ProviderUtil.firstNonEmpty(objStr(meta, "subject"), raw["subject"] as String)
            raw["text"] = htmlToText(html)
            raw["html"] = html
        } catch (_: Exception) {
            // 保留摘要
        }
        return raw
    }

    /** 将 HTML 转为纯文本（移除标签并解码常见实体）。 */
    private fun htmlToText(html: String?): String {
        if (html.isNullOrEmpty()) return ""
        return tagRe.replace(html, " ")
            .replace("&nbsp;", " ").replace("&amp;", "&")
            .replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"")
            .replace(Regex("\\s+"), " ").trim()
    }
}
