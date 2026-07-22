package com.xxxxteam.tempmail

import kotlinx.serialization.json.*

/**
 * 各字段的多候选键名常量数组
 *
 * 提取为顶层不可变常量，避免每次 [Normalize.fromJson] / [Normalize.fromMap]
 * 调用时因 vararg 而重复分配字符串数组。
 */
private val KEYS_ID = arrayOf("id", "mail_id", "message_id", "uid", "seq")
private val KEYS_FROM = arrayOf("from", "from_address", "sender", "mail_sender", "sender_email", "mail_from")
private val KEYS_TO = arrayOf("to", "to_address", "recipient", "mail_recipient", "mail_to")
private val KEYS_SUBJECT = arrayOf("subject", "mail_subject", "title", "mail_title")
private val KEYS_BODY = arrayOf("body", "text", "content", "html", "mail_body", "mail_html", "textBody", "htmlBody", "body_text", "body_html")
private val KEYS_DATE = arrayOf("date", "created_at", "received_at", "time", "timestamp", "mail_date", "receivedAt", "createdAt")

/**
 * 邮件标准化工具
 *
 * 将各渠道异构的 API 响应映射为统一 Email 结构，
 * 使用多候选字段策略提取字段值。
 */
object Normalize {

    /** 从 JSON 对象中提取标准化邮件 */
    fun fromJson(obj: JsonObject, recipient: String = ""): Email {
        return Email(
            id = extractString(obj, KEYS_ID),
            from = extractString(obj, KEYS_FROM),
            to = extractString(obj, KEYS_TO).ifEmpty { recipient },
            subject = extractString(obj, KEYS_SUBJECT),
            body = extractString(obj, KEYS_BODY),
            date = extractString(obj, KEYS_DATE),
        )
    }

    /** 从 Map 中提取标准化邮件 */
    fun fromMap(map: Map<String, Any?>, recipient: String = ""): Email {
        return Email(
            id = extractFromMap(map, KEYS_ID),
            from = extractFromMap(map, KEYS_FROM),
            to = extractFromMap(map, KEYS_TO).ifEmpty { recipient },
            subject = extractFromMap(map, KEYS_SUBJECT),
            body = extractFromMap(map, KEYS_BODY),
            date = extractFromMap(map, KEYS_DATE),
        )
    }

    /** 多候选字段提取（JsonObject），keys 为预分配的常量数组 */
    private fun extractString(obj: JsonObject, keys: Array<String>): String {
        for (key in keys) {
            val value = obj[key]
            if (value != null && value !is JsonNull) {
                return when (value) {
                    is JsonPrimitive -> value.content
                    else -> value.toString()
                }
            }
        }
        return ""
    }

    /** 多候选字段提取（Map），keys 为预分配的常量数组 */
    private fun extractFromMap(map: Map<String, Any?>, keys: Array<String>): String {
        for (key in keys) {
            val value = map[key]
            if (value != null) {
                return value.toString()
            }
        }
        return ""
    }

    /** HTML 标签正则 */
    private val TAG_RE = Regex("<[^>]+>")
    private val MULTI_SPACE_RE = Regex("\\s{2,}")
    // 预编译换行相关标签正则，避免 htmlToText 每次调用重复编译
    private val BR_RE = Regex("(?i)<br\\s*/?>")
    private val P_END_RE = Regex("(?i)</p>")
    private val DIV_END_RE = Regex("(?i)</div>")

    /**
     * HTML 转纯文本
     *
     * 去除所有标签，解码常见 HTML 实体，压缩多余空白。
     * 用于 GuerrillaMail / Moakt 等返回 HTML body 的渠道。
     */
    fun htmlToText(html: String): String {
        if (html.isBlank()) return ""
        return html
            .replace(BR_RE, "\n")
            .replace(P_END_RE, "\n")
            .replace(DIV_END_RE, "\n")
            .replace(TAG_RE, "")
            .replace("&nbsp;", " ")
            .replace("&amp;", "&")
            .replace("&lt;", "<")
            .replace("&gt;", ">")
            .replace("&quot;", "\"")
            .replace("&#39;", "'")
            .replace(MULTI_SPACE_RE, " ")
            .trim()
    }
}
