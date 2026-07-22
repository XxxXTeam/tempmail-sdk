package com.xxxxteam.tempmail.providers

import com.xxxxteam.tempmail.Http
import io.ktor.client.plugins.*
import io.ktor.client.request.*
import io.ktor.client.statement.*
import io.ktor.http.*
import kotlinx.serialization.json.*
import java.net.URLEncoder
import java.nio.charset.StandardCharsets
import java.util.Base64
import java.util.concurrent.ConcurrentHashMap
import kotlin.random.Random

/**
 * HTTP 响应封装。
 *
 * @property statusCode HTTP 状态码
 * @property body 响应体文本
 * @property headers 响应头映射（键小写）
 * @property setCookies 响应中的所有 Set-Cookie 头
 */
internal data class HttpResp(
    val statusCode: Int,
    val body: String,
    val headers: Map<String, String> = emptyMap(),
    val setCookies: List<String> = emptyList(),
) {
    /** 是否 2xx 成功。 */
    val isOk: Boolean get() = statusCode in 200..299

    /** 断言成功，否则抛出异常。 */
    fun ensureSuccess() {
        if (!isOk) throw RuntimeException("HTTP 请求失败: 状态码 $statusCode")
    }

    /** 获取 Location 响应头（302 跳转常用） */
    val location: String get() = headers["location"] ?: headers["Location"] ?: ""
}

/**
 * provider 通用工具：随机串、URL 编码、非空择取、HTTP 请求封装。
 *
 * 所有 HTTP 请求统一走 [Http.client]，以复用代理/TLS/超时配置。
 */
internal object ProviderUtil {

    private const val CHARS = "abcdefghijklmnopqrstuvwxyz0123456789"

    /**
     * 生成指定长度的随机小写字母数字串。
     *
     * @param len 长度
     * @return 随机串
     */
    fun randomString(len: Int): String {
        val sb = StringBuilder(len)
        repeat(len) { sb.append(CHARS[Random.nextInt(CHARS.length)]) }
        return sb.toString()
    }

    /** 生成以 "sdk" 开头再拼接 16 位随机串的本地名。 */
    fun randomLocalSdk(): String = "sdk" + randomString(16)

    /**
     * URL 编码。
     *
     * @param s 原字符串
     * @return 编码结果
     */
    fun urlEncode(s: String?): String = URLEncoder.encode(s ?: "", StandardCharsets.UTF_8)

    /**
     * 返回第一个非空白字符串，全空返回空串。
     *
     * @param values 候选值
     * @return 首个非空白值
     */
    fun firstNonEmpty(vararg values: String?): String {
        for (v in values) {
            if (!v.isNullOrBlank()) return v
        }
        return ""
    }

    /**
     * 发起 GET 请求。
     *
     * @param url 请求地址
     * @param headers 请求头
     * @param followRedirects 是否跟随重定向（默认 true）
     * @return HTTP 响应封装
     */
    suspend fun httpGet(
        url: String,
        headers: Map<String, String> = emptyMap(),
        followRedirects: Boolean = true,
    ): HttpResp {
        val client = if (followRedirects) Http.client else Http.client.config { this.followRedirects = false }
        val resp = client.get(url) {
            expectSuccess = false
            headers.forEach { (k, v) -> header(k, v) }
        }
        return resp.toResp()
    }

    /**
     * 发起 POST 请求。
     *
     * @param url 请求地址
     * @param body 请求体，可为 null
     * @param contentType Content-Type，可为 null
     * @param headers 请求头（其中的 Content-Type 会被 contentType 参数覆盖）
     * @param followRedirects 是否跟随重定向（默认 true）
     * @return HTTP 响应封装
     */
    suspend fun httpPost(
        url: String,
        body: String? = null,
        contentType: String? = null,
        headers: Map<String, String> = emptyMap(),
        followRedirects: Boolean = true,
    ): HttpResp {
        val client = if (followRedirects) Http.client else Http.client.config { this.followRedirects = false }
        val resp = client.post(url) {
            expectSuccess = false
            headers.forEach { (k, v) ->
                if (contentType == null || !k.equals("Content-Type", ignoreCase = true)) header(k, v)
            }
            if (contentType != null) contentType(ContentType.parse(contentType))
            if (body != null) setBody(body)
        }
        return resp.toResp()
    }

    /** 将 ktor 响应转换为内部封装。 */
    private suspend fun HttpResponse.toResp(): HttpResp {
        val respHeaders = mutableMapOf<String, String>()
        headers.forEach { name, values -> respHeaders[name.lowercase()] = values.joinToString(", ") }
        return HttpResp(
            statusCode = status.value,
            body = bodyAsText(),
            headers = respHeaders,
            setCookies = headers.getAll(HttpHeaders.SetCookie) ?: emptyList(),
        )
    }

    /**
     * 宽松解析 JSON 文本为 JsonElement，失败返回 null。
     *
     * @param text JSON 文本
     * @return JsonElement 或 null
     */
    fun parse(text: String?): JsonElement? {
        if (text.isNullOrBlank()) return null
        return try {
            Http.json.parseToJsonElement(text)
        } catch (_: Exception) {
            null
        }
    }

    /**
     * 宽松解析 JSON 文本为 JsonObject，失败返回 null。
     *
     * @param text JSON 文本
     * @return JsonObject 或 null
     */
    fun parseObject(text: String?): JsonObject? = parse(text) as? JsonObject

    /**
     * 从 JsonObject 取字符串字段（数字/布尔自动转文本），缺失返回空串。
     *
     * @param obj JSON 对象
     * @param key 字段名
     * @return 字符串值
     */
    fun str(obj: JsonObject?, key: String): String {
        val v = obj?.get(key) ?: return ""
        if (v is JsonNull) return ""
        return (v as? JsonPrimitive)?.content ?: v.toString()
    }

    /**
     * 从 JsonObject 取数组字段，缺失或类型不符返回 null。
     *
     * @param obj JSON 对象
     * @param key 字段名
     * @return JsonArray 或 null
     */
    fun arr(obj: JsonObject?, key: String): JsonArray? = obj?.get(key) as? JsonArray

    /**
     * 从 JsonObject 取布尔字段，兼容 "1"/"true" 字符串。
     *
     * @param obj JSON 对象
     * @param key 字段名
     * @return 布尔值
     */
    fun bool(obj: JsonObject?, key: String): Boolean {
        val v = obj?.get(key) as? JsonPrimitive ?: return false
        return v.booleanOrNull ?: (v.content == "1" || v.content.equals("true", ignoreCase = true))
    }

    // ==================== Cookie / CSRF / 正则 / Base64 工具 ====================
    // 由 kotlin-providers-4 追加，供依赖 cookie/CSRF 会话的渠道复用。

    /** CSRF meta 标签正则，预编译避免重复编译。 */
    private val CSRF_RE = Regex("<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"")

    /** 动态正则编译缓存，避免 [regexExtract] 重复编译相同表达式。 */
    private val PATTERN_CACHE = ConcurrentHashMap<String, Regex>()

    /**
     * 从 Set-Cookie 列表中提取所有 cookie，拼接为 "k=v; k2=v2" 格式。
     *
     * 接收 [HttpResp.setCookies]，兼容 ProviderUtil.httpGet/httpPost 返回类型。
     *
     * @param setCookies Set-Cookie 原始值列表
     * @return cookie 字符串
     */
    fun extractAllCookies(setCookies: List<String>): String {
        val map = LinkedHashMap<String, String>()
        for (raw in setCookies) {
            val pair = raw.split(";", limit = 2)[0].trim()
            val eq = pair.indexOf('=')
            if (eq > 0) {
                map[pair.substring(0, eq).trim()] = pair.substring(eq + 1).trim()
            }
        }
        return map.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }

    /**
     * 从 HttpResp 中提取所有 cookie（[extractAllCookies] 的便捷重载）。
     *
     * @param resp HTTP 响应
     * @return cookie 字符串
     */
    fun extractAllCookies(resp: HttpResp): String = extractAllCookies(resp.setCookies)

    /**
     * 合并两个 cookie 字符串（新 cookie 覆盖旧的同名键）。
     *
     * @param existing 已有 cookie 串
     * @param newer 新 cookie 串
     * @return 合并后的 cookie 串
     */
    fun mergeCookieStrings(existing: String?, newer: String?): String {
        val map = LinkedHashMap<String, String>()
        parseCookieString(existing, map)
        parseCookieString(newer, map)
        return map.entries.joinToString("; ") { "${it.key}=${it.value}" }
    }

    /** 解析 cookie 串并写入 out（同名键覆盖）。 */
    private fun parseCookieString(cookieStr: String?, out: MutableMap<String, String>) {
        if (cookieStr.isNullOrEmpty()) return
        for (part in cookieStr.split(";")) {
            val trimmed = part.trim()
            val eq = trimmed.indexOf('=')
            if (eq > 0) {
                out[trimmed.substring(0, eq).trim()] = trimmed.substring(eq + 1).trim()
            }
        }
    }

    /**
     * 从 HTML 中用正则提取 CSRF token（meta name="csrf-token" content="xxx"）。
     *
     * @param html HTML 文本
     * @return CSRF token，未匹配返回空串
     */
    fun extractCsrfToken(html: String?): String {
        if (html.isNullOrEmpty()) return ""
        return CSRF_RE.find(html)?.groupValues?.get(1) ?: ""
    }

    /**
     * 用指定正则从文本中提取第一个捕获组（带编译缓存）。
     *
     * @param text 源文本
     * @param pattern 正则表达式（含一个捕获组）
     * @return 匹配内容，未匹配返回空串
     */
    fun regexExtract(text: String?, pattern: String): String {
        if (text.isNullOrEmpty()) return ""
        val re = PATTERN_CACHE.computeIfAbsent(pattern) { Regex(it) }
        return re.find(text)?.groupValues?.getOrNull(1) ?: ""
    }

    /**
     * Base64 编码。
     *
     * @param data 原始字节
     * @return base64 字符串
     */
    fun base64Encode(data: ByteArray): String = Base64.getEncoder().encodeToString(data)

    /**
     * Base64 解码。
     *
     * @param encoded base64 字符串
     * @return 原始字节
     */
    fun base64Decode(encoded: String): ByteArray = Base64.getDecoder().decode(encoded)
}
