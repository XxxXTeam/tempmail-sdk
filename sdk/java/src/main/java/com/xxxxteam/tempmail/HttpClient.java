package com.xxxxteam.tempmail;

import java.net.InetSocketAddress;
import java.net.ProxySelector;
import java.net.URI;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.security.SecureRandom;
import java.security.cert.X509Certificate;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.concurrent.ThreadLocalRandom;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

/**
 * 共享 HTTP 客户端。所有 provider 通过此类发起请求，自动应用全局配置
 * （代理、超时、TLS）。底层 {@link java.net.http.HttpClient} 内部缓存复用，
 * 仅在配置版本变更时重建，保证连接池复用。
 */
public final class HttpClient {

    private static final Object LOCK = new Object();
    private static java.net.http.HttpClient cached;
    private static int cachedVersion = -1;
    private static volatile int cachedTimeout = 15;

    // 不跟随重定向的客户端同样按配置版本缓存，避免每次请求重建（重建会丢失连接池复用）
    private static final Object NR_LOCK = new Object();
    private static java.net.http.HttpClient cachedNoRedirect;
    private static int cachedNoRedirectVersion = -1;

    private HttpClient() {
    }

    /**
     * 生成随机 IPv4 地址，用于伪造来源 IP 请求头。
     *
     * @return 点分十进制 IPv4 字符串
     */
    private static String randomIpv4() {
        ThreadLocalRandom r = ThreadLocalRandom.current();
        return r.nextInt(1, 255) + "." + r.nextInt(1, 255) + "." + r.nextInt(1, 255) + "." + r.nextInt(1, 255);
    }

    /**
     * 获取（或按配置版本重建）共享底层客户端。
     *
     * @return 底层 HttpClient
     */
    private static java.net.http.HttpClient getClient() {
        synchronized (LOCK) {
            int version = Config.getVersion();
            if (cached != null && cachedVersion == version) {
                return cached;
            }

            SdkConfig cfg = Config.get();
            java.net.http.HttpClient.Builder builder = java.net.http.HttpClient.newBuilder()
                    .followRedirects(java.net.http.HttpClient.Redirect.NORMAL)
                    .connectTimeout(Duration.ofSeconds(Math.max(1, cfg.getTimeout())));

            applyProxy(builder, cfg);

            if (cfg.isInsecure()) {
                SSLContext ctx = buildInsecureSsl();
                if (ctx != null) {
                    builder.sslContext(ctx);
                }
            }

            cached = builder.build();
            cachedVersion = version;
            cachedTimeout = cfg.getTimeout();
            return cached;
        }
    }

    /**
     * 获取（或按配置版本重建）不跟随重定向的共享客户端，复用连接池。
     *
     * @return 底层 HttpClient（Redirect.NEVER）
     */
    private static java.net.http.HttpClient getNoRedirectClient() {
        synchronized (NR_LOCK) {
            int version = Config.getVersion();
            if (cachedNoRedirect != null && cachedNoRedirectVersion == version) {
                return cachedNoRedirect;
            }
            SdkConfig cfg = Config.get();
            java.net.http.HttpClient.Builder builder = java.net.http.HttpClient.newBuilder()
                    .followRedirects(java.net.http.HttpClient.Redirect.NEVER)
                    .connectTimeout(Duration.ofSeconds(Math.max(1, cfg.getTimeout())));
            applyProxy(builder, cfg);
            if (cfg.isInsecure()) {
                SSLContext ctx = buildInsecureSsl();
                if (ctx != null) {
                    builder.sslContext(ctx);
                }
            }
            cachedNoRedirect = builder.build();
            cachedNoRedirectVersion = version;
            cachedTimeout = cfg.getTimeout();
            return cachedNoRedirect;
        }
    }

    /**
     * 将配置中的代理应用到客户端构造器（解析失败则不使用代理）。
     *
     * @param builder 客户端构造器
     * @param cfg     当前配置
     */
    private static void applyProxy(java.net.http.HttpClient.Builder builder, SdkConfig cfg) {
        String proxy = cfg.getProxy();
        if (proxy == null || proxy.isBlank()) {
            return;
        }
        try {
            URI u = URI.create(proxy.trim());
            int port = u.getPort();
            if (port < 0) {
                port = "https".equalsIgnoreCase(u.getScheme()) ? 443 : 80;
            }
            if (u.getHost() != null) {
                builder.proxy(ProxySelector.of(new InetSocketAddress(u.getHost(), port)));
            }
        } catch (RuntimeException ignored) {
            // 代理解析失败则不使用代理
        }
    }

    /**
     * 构造信任所有证书的 SSLContext（调试用）。
     *
     * @return SSLContext，失败返回 null
     */
    private static SSLContext buildInsecureSsl() {
        try {
            TrustManager[] trustAll = {
                new X509TrustManager() {
                    @Override
                    public void checkClientTrusted(X509Certificate[] chain, String authType) {
                    }

                    @Override
                    public void checkServerTrusted(X509Certificate[] chain, String authType) {
                    }

                    @Override
                    public X509Certificate[] getAcceptedIssuers() {
                        return new X509Certificate[0];
                    }
                }
            };
            SSLContext ctx = SSLContext.getInstance("TLS");
            ctx.init(null, trustAll, new SecureRandom());
            return ctx;
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * 发起 GET 请求。
     *
     * @param url 请求地址
     * @return 响应结果
     */
    public static HttpResult get(String url) {
        return send("GET", url, null, null, null, null);
    }

    /**
     * 发起 GET 请求（带请求头）。
     *
     * @param url     请求地址
     * @param headers 请求级请求头，可为 null
     * @return 响应结果
     */
    public static HttpResult get(String url, Map<String, String> headers) {
        return send("GET", url, headers, null, null, null);
    }

    /**
     * 发起 POST 请求。
     *
     * @param url         请求地址
     * @param body        请求正文，可为 null
     * @param contentType 内容类型，可为 null
     * @param headers     请求级请求头，可为 null
     * @return 响应结果
     */
    public static HttpResult post(String url, String body, String contentType, Map<String, String> headers) {
        return send("POST", url, headers, body, contentType, null);
    }

    /**
     * 发起 POST 请求且不跟随重定向（用于需要捕获 3xx 响应 Cookie 的场景）。
     *
     * @param url         请求地址
     * @param body        请求正文，可为 null
     * @param contentType 内容类型，可为 null
     * @param headers     请求级请求头，可为 null
     * @return 响应结果（状态码可能为 3xx）
     */
    public static HttpResult postNoRedirect(String url, String body, String contentType, Map<String, String> headers) {
        return sendNoRedirect("POST", url, headers, body, contentType, null);
    }

    /**
     * 不跟随重定向的通用请求实现，用于需要捕获 302 中间响应头（如 Set-Cookie）的场景。
     *
     * @param method      HTTP 方法
     * @param url         请求地址
     * @param headers     请求级请求头，可为 null
     * @param body        请求正文，可为 null
     * @param contentType 内容类型，可为 null
     * @param timeout     单请求超时秒数，可为 null
     * @return 响应结果
     */
    public static HttpResult sendNoRedirect(String method, String url, Map<String, String> headers,
                                            String body, String contentType, Integer timeout) {
        // 复用按配置版本缓存的 NEVER 重定向客户端，保留连接池
        java.net.http.HttpClient client = getNoRedirectClient();
        int effectiveTimeout = timeout != null ? timeout : cachedTimeout;
        SdkConfig cfg = Config.get();

        HttpRequest.Builder req = HttpRequest.newBuilder()
                .uri(URI.create(url))
                .timeout(Duration.ofSeconds(Math.max(1, effectiveTimeout)));
        String ip = randomIpv4();
        req.header("X-Forwarded-For", ip);
        req.header("X-Real-IP", ip);
        req.header("X-Originating-IP", ip);
        if (cfg.getHeaders() != null) {
            for (Map.Entry<String, String> kv : cfg.getHeaders().entrySet()) {
                safeHeader(req, kv.getKey(), kv.getValue());
            }
        }
        if (headers != null) {
            for (Map.Entry<String, String> kv : headers.entrySet()) {
                safeHeader(req, kv.getKey(), kv.getValue());
            }
        }
        HttpRequest.BodyPublisher publisher = body != null
                ? HttpRequest.BodyPublishers.ofString(body)
                : HttpRequest.BodyPublishers.noBody();
        req.method(method, publisher);
        if (body != null && contentType != null && !contentType.isEmpty()) {
            safeHeader(req, "Content-Type", contentType);
        }
        HttpResponse<String> resp;
        try {
            resp = client.send(req.build(), HttpResponse.BodyHandlers.ofString());
        } catch (java.net.http.HttpTimeoutException e) {
            throw new RuntimeException("request timed out: " + url, e);
        } catch (Exception e) {
            throw new RuntimeException("connection error: " + e.getMessage(), e);
        }
        Map<String, String> respHeaders = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        List<String> setCookies = new ArrayList<>();
        resp.headers().map().forEach((k, values) -> {
            if ("set-cookie".equalsIgnoreCase(k)) {
                setCookies.addAll(values);
            } else {
                respHeaders.put(k, String.join(", ", values));
            }
        });
        return new HttpResult(resp.statusCode(), resp.body(), respHeaders, setCookies);
    }

    /**
     * 通用请求实现（同步阻塞，provider 逻辑为同步风格）。
     *
     * @param method      HTTP 方法
     * @param url         请求地址
     * @param headers     请求级请求头，可为 null
     * @param body        请求正文，可为 null
     * @param contentType 内容类型，可为 null
     * @param timeout     单请求超时秒数，可为 null
     * @return 响应结果
     */
    public static HttpResult send(String method, String url, Map<String, String> headers,
                                  String body, String contentType, Integer timeout) {
        java.net.http.HttpClient client = getClient();
        int effectiveTimeout = timeout != null ? timeout : cachedTimeout;

        HttpRequest.Builder req = HttpRequest.newBuilder()
                .uri(URI.create(url))
                .timeout(Duration.ofSeconds(Math.max(1, effectiveTimeout)));

        // 伪造来源 IP 头
        String ip = randomIpv4();
        req.header("X-Forwarded-For", ip);
        req.header("X-Real-IP", ip);
        req.header("X-Originating-IP", ip);

        // 全局自定义头
        SdkConfig cfg = Config.get();
        if (cfg.getHeaders() != null) {
            for (Map.Entry<String, String> kv : cfg.getHeaders().entrySet()) {
                safeHeader(req, kv.getKey(), kv.getValue());
            }
        }

        // 请求级头
        if (headers != null) {
            for (Map.Entry<String, String> kv : headers.entrySet()) {
                safeHeader(req, kv.getKey(), kv.getValue());
            }
        }

        HttpRequest.BodyPublisher publisher = body != null
                ? HttpRequest.BodyPublishers.ofString(body)
                : HttpRequest.BodyPublishers.noBody();
        req.method(method, publisher);
        if (body != null && contentType != null && !contentType.isEmpty()) {
            safeHeader(req, "Content-Type", contentType);
        }

        HttpResponse<String> resp;
        try {
            resp = client.send(req.build(), HttpResponse.BodyHandlers.ofString());
        } catch (java.net.http.HttpTimeoutException e) {
            throw new RuntimeException("request timed out: " + url, e);
        } catch (Exception e) {
            throw new RuntimeException("connection error: " + e.getMessage(), e);
        }

        Map<String, String> respHeaders = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        List<String> setCookies = new ArrayList<>();
        resp.headers().map().forEach((k, values) -> {
            if ("set-cookie".equalsIgnoreCase(k)) {
                setCookies.addAll(values);
            } else {
                respHeaders.put(k, String.join(", ", values));
            }
        });

        return new HttpResult(resp.statusCode(), resp.body(), respHeaders, setCookies);
    }

    /**
     * 安全添加请求头，忽略 JDK 限制的受限头（如 Host、Content-Length）。
     *
     * @param req   请求构造器
     * @param name  头名
     * @param value 头值
     */
    private static void safeHeader(HttpRequest.Builder req, String name, String value) {
        if (name == null || value == null) {
            return;
        }
        try {
            req.header(name, value);
        } catch (IllegalArgumentException ignored) {
            // JDK 限制的受限头静默忽略
        }
    }
}
