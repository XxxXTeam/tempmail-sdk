package com.xxxxteam.tempmail.providers

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import java.util.Base64

/**
 * Cookie / CSRF / Base64 辅助工具（第4批后半 provider 专用）。
 *
 * 与 Java 端 ProviderUtil 的对应方法语义完全一致，独立成文件避免与
 * [ProviderUtil] 重复定义同名方法。
 */
internal object CookieHelpers {

    private val CSRF_RE = Regex("<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"")

    /**
     * 从响应的所有 Set-Cookie 头提取 `k=v` 拼成 Cookie 串（后者覆盖同名键）。
     *
     * @param resp HTTP 响应封装
     * @return Cookie 串，如 "a=1; b=2"
     */
    fun extractAllCookies(resp: HttpResp): String {
        val map = LinkedHashMap<String, String>()
        for (raw in resp.setCookies) {
            val pair = raw.substringBefore(";").trim()
            val eq = pair.indexOf('=')
            if (eq > 0) map[pair.substring(0, eq).trim()] = pair.substring(eq + 1).trim()
        }
        return map.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }

    /**
     * 合并两个 Cookie 串（newer 覆盖 existing 的同名键）。
     *
     * @param existing 已有 Cookie 串
     * @param newer 新 Cookie 串
     * @return 合并后的 Cookie 串
     */
    fun mergeCookieStrings(existing: String, newer: String): String {
        val map = LinkedHashMap<String, String>()
        parseInto(existing, map)
        parseInto(newer, map)
        return map.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }

    private fun parseInto(cookieStr: String?, out: LinkedHashMap<String, String>) {
        if (cookieStr.isNullOrEmpty()) return
        for (part in cookieStr.split(";")) {
            val t = part.trim()
            val eq = t.indexOf('=')
            if (eq > 0) out[t.substring(0, eq).trim()] = t.substring(eq + 1).trim()
        }
    }

    /**
     * 从 HTML 中提取 `<meta name="csrf-token" content="...">`。
     *
     * @param html HTML 文本
     * @return CSRF token，未匹配返回空串
     */
    fun extractCsrfToken(html: String): String =
        CSRF_RE.find(html)?.groupValues?.get(1) ?: ""

    /**
     * 用指定正则提取第一个捕获组。
     *
     * @param text 源文本
     * @param pattern 正则表达式（含一个捕获组）
     * @return 匹配内容，未匹配返回空串
     */
    fun regexExtract(text: String, pattern: String): String =
        Regex(pattern).find(text)?.groupValues?.getOrNull(1) ?: ""

    /** Base64 编码。 */
    fun base64Encode(data: ByteArray): String = Base64.getEncoder().encodeToString(data)

    /** Base64 解码。 */
    fun base64Decode(encoded: String): ByteArray = Base64.getDecoder().decode(encoded)

    /** 从 JsonObject 读取字符串（原始值直接取 content），缺失返回空串。 */
    fun str(obj: JsonObject?, key: String): String =
        (obj?.get(key) as? JsonPrimitive)?.content ?: ""
}
