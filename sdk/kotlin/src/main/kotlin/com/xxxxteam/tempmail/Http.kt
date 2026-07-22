package com.xxxxteam.tempmail

import io.ktor.client.*
import io.ktor.client.engine.cio.*
import io.ktor.client.plugins.*
import io.ktor.client.plugins.contentnegotiation.*
import io.ktor.client.plugins.websocket.*
import io.ktor.client.request.*
import io.ktor.client.statement.*
import io.ktor.http.*
import io.ktor.serialization.kotlinx.json.*
import kotlinx.serialization.json.Json
import java.security.cert.X509Certificate
import javax.net.ssl.X509TrustManager

/**
 * HTTP 响应封装
 *
 * @property statusCode HTTP 状态码
 * @property body 响应正文（UTF-8）
 * @property headers 响应头映射
 * @property setCookies 所有 Set-Cookie 原始值
 */
data class HttpResult(
    val statusCode: Int,
    val body: String,
    val headers: Map<String, String> = emptyMap(),
    val setCookies: List<String> = emptyList(),
) {
    /** 状态码是否在 2xx 范围 */
    val isOk: Boolean get() = statusCode in 200..299

    /** 非 2xx 时抛出异常 */
    fun ensureSuccess(): HttpResult {
        if (!isOk) throw RuntimeException("HTTP 请求失败: $statusCode")
        return this
    }
}

/**
 * 全局 HTTP 客户端封装
 *
 * 基于 ktor-client CIO 引擎，支持代理、TLS 配置、超时和 WebSocket。
 * 所有 provider 必须使用此客户端发起 HTTP 请求。
 */
object Http {
    /** 宽松 JSON 解析器（忽略未知字段、允许宽松数字等） */
    val json = Json {
        ignoreUnknownKeys = true
        isLenient = true
        coerceInputValues = true
        explicitNulls = false
    }

    @Volatile
    private var _client: HttpClient? = null

    /** 获取共享 HTTP 客户端实例 */
    val client: HttpClient
        get() = _client ?: synchronized(this) {
            _client ?: buildClient().also { _client = it }
        }

    /** 重建客户端（配置变更时调用） */
    fun rebuildClient() {
        synchronized(this) {
            _client?.close()
            _client = buildClient()
        }
    }

    private fun buildClient(): HttpClient {
        // 仅只读读取配置，零拷贝
        val cfg = Config.current()
        return HttpClient(CIO) {
            engine {
                requestTimeout = cfg.timeout * 1000L
                // 连接池调优：提高整体并发连接上限与每主机连接数，延长 keep-alive 以复用连接
                maxConnectionsCount = 1000
                endpoint {
                    maxConnectionsPerRoute = 100
                    pipelineMaxSize = 20
                    keepAliveTime = 30_000
                    connectTimeout = cfg.timeout * 1000L
                    connectAttempts = 1
                }
                if (cfg.insecure) {
                    https {
                        trustManager = object : X509TrustManager {
                            override fun checkClientTrusted(chain: Array<out X509Certificate>?, authType: String?) {}
                            override fun checkServerTrusted(chain: Array<out X509Certificate>?, authType: String?) {}
                            override fun getAcceptedIssuers(): Array<X509Certificate> = emptyArray()
                        }
                    }
                }
                proxy = cfg.proxy?.let { proxyUrl ->
                    when {
                        proxyUrl.startsWith("http") -> io.ktor.client.engine.ProxyBuilder.http(Url(proxyUrl))
                        proxyUrl.startsWith("socks") -> io.ktor.client.engine.ProxyBuilder.socks(
                            proxyUrl.substringAfter("://").substringBefore(":"),
                            proxyUrl.substringAfterLast(":").toIntOrNull() ?: 1080
                        )
                        else -> null
                    }
                }
            }
            install(ContentNegotiation) {
                json(json)
            }
            install(WebSockets)
            install(HttpTimeout) {
                requestTimeoutMillis = cfg.timeout * 1000L
                connectTimeoutMillis = cfg.timeout * 1000L
            }
            defaultRequest {
                header(HttpHeaders.UserAgent, "TempMailSDK-Kotlin/0.1.0")
            }
        }
    }

    /**
     * 便捷 GET 请求，返回 HttpResult 封装
     *
     * @param url 请求地址
     * @param headers 附加请求头
     */
    suspend fun get(url: String, headers: Map<String, String> = emptyMap()): HttpResult {
        val response = client.get(url) {
            headers.forEach { (k, v) -> header(k, v) }
        }
        return response.toHttpResult()
    }

    /**
     * 便捷 POST 请求，返回 HttpResult 封装
     *
     * @param url 请求地址
     * @param body 请求正文
     * @param contentType 内容类型
     * @param headers 附加请求头
     */
    suspend fun post(
        url: String,
        body: String? = null,
        contentType: String = "application/json",
        headers: Map<String, String> = emptyMap(),
    ): HttpResult {
        val response = client.post(url) {
            headers.forEach { (k, v) -> header(k, v) }
            if (body != null) {
                contentType(ContentType.parse(contentType))
                setBody(body)
            }
        }
        return response.toHttpResult()
    }

    /**
     * POST 请求但不跟随重定向（用于捕获 3xx 响应的 Set-Cookie）
     */
    suspend fun postNoRedirect(
        url: String,
        body: String? = null,
        contentType: String = "application/json",
        headers: Map<String, String> = emptyMap(),
    ): HttpResult {
        val noRedirectClient = HttpClient(CIO) {
            followRedirects = false
            install(HttpTimeout) {
                val cfg = Config.current()
                requestTimeoutMillis = cfg.timeout * 1000L
                connectTimeoutMillis = cfg.timeout * 1000L
            }
        }
        val response = noRedirectClient.use { c ->
            c.post(url) {
                headers.forEach { (k, v) -> header(k, v) }
                if (body != null) {
                    contentType(ContentType.parse(contentType))
                    setBody(body)
                }
            }
        }
        return response.toHttpResult()
    }

    /**
     * GET 请求但不跟随重定向
     */
    suspend fun getNoRedirect(
        url: String,
        headers: Map<String, String> = emptyMap(),
    ): HttpResult {
        val noRedirectClient = HttpClient(CIO) {
            followRedirects = false
            install(HttpTimeout) {
                val cfg = Config.current()
                requestTimeoutMillis = cfg.timeout * 1000L
                connectTimeoutMillis = cfg.timeout * 1000L
            }
        }
        val response = noRedirectClient.use { c ->
            c.get(url) {
                headers.forEach { (k, v) -> header(k, v) }
            }
        }
        return response.toHttpResult()
    }
}

/** HttpResponse 转 HttpResult 的扩展函数 */
private suspend fun HttpResponse.toHttpResult(): HttpResult {
    val respBody = bodyAsText()
    val respHeaders = mutableMapOf<String, String>()
    headers.forEach { name, values -> respHeaders[name] = values.joinToString(", ") }
    val cookies = headers.getAll(HttpHeaders.SetCookie) ?: emptyList()
    return HttpResult(
        statusCode = status.value,
        body = respBody,
        headers = respHeaders,
        setCookies = cookies,
    )
}
