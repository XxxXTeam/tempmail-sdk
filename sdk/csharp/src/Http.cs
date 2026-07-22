using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Http;
using System.Net.Security;
using System.Text;
using System.Threading;

namespace XxxXTeam.TempMail;

/// <summary>
/// HTTP 响应封装，供 provider 读取状态码、正文与响应头。
/// </summary>
public sealed class HttpResult
{
    /// <summary>HTTP 状态码</summary>
    public int StatusCode { get; init; }

    /// <summary>响应正文（UTF-8 文本）</summary>
    public string Body { get; init; } = "";

    /// <summary>响应头（不含 Set-Cookie 原始值时以 SetCookies 提供）</summary>
    public Dictionary<string, string> Headers { get; init; } = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>所有 Set-Cookie 原始值</summary>
    public List<string> SetCookies { get; init; } = new();

    /// <summary>状态码是否在 2xx 范围</summary>
    public bool Ok => StatusCode is >= 200 and < 300;

    /// <summary>状态码非 2xx 时抛出异常（消息含状态码，供 retry 判定）</summary>
    public void EnsureSuccess()
    {
        if (!Ok)
            throw new HttpRequestException($"HTTP request failed: {StatusCode}");
    }
}

/// <summary>
/// 共享 HTTP 客户端。所有 provider 通过此模块发起请求，自动应用全局配置
/// （代理、超时、TLS）。HttpClient 内部缓存复用，仅在配置版本变更时重建，
/// 保证连接池复用。
/// </summary>
public static class Http
{
    private static readonly object _lock = new();
    private static HttpClient? _cached;
    private static int _cachedVersion = -1;
    // 不跟随重定向的客户端缓存（供 xkx-me 等需要读取 302 Location 头的渠道使用）
    private static HttpClient? _cachedNoRedirect;
    private static int _cachedNoRedirectVersion = -1;
    private static volatile int _cachedTimeout = 15;

    // moakt 等 HTML 抓取渠道专用的裸客户端：禁用内部 Cookie 容器（手动管理 Cookie 头），
    // 按是否跟随重定向分两份缓存；仍复用全局 Config（代理/TLS/超时/自定义头）。
    private static HttpClient? _cachedRawRedirect;
    private static HttpClient? _cachedRawNoRedirect;
    private static int _cachedRawVersion = -1;

    private static string RandomIpv4()
    {
        // 每段 1-254，用于伪造来源 IP 请求头；Random.Shared 线程安全，免去共享 Random 加锁与竞态
        var r = Random.Shared;
        return $"{r.Next(1, 255)}.{r.Next(1, 255)}.{r.Next(1, 255)}.{r.Next(1, 255)}";
    }

    private static HttpClient GetClient()
    {
        lock (_lock)
        {
            var version = Config.Version;
            if (_cached is not null && _cachedVersion == version)
                return _cached;

            var client = BuildClient(true);
            _cached?.Dispose();
            _cached = client;
            _cachedVersion = version;
            return client;
        }
    }

    private static HttpClient GetClientNoRedirect()
    {
        lock (_lock)
        {
            var version = Config.Version;
            if (_cachedNoRedirect is not null && _cachedNoRedirectVersion == version)
                return _cachedNoRedirect;

            var client = BuildClient(false);
            _cachedNoRedirect?.Dispose();
            _cachedNoRedirect = client;
            _cachedNoRedirectVersion = version;
            return client;
        }
    }

    /// <summary>
    /// 获取裸客户端（禁用内部 Cookie 容器，供手动管理 Cookie 头的 HTML 抓取渠道使用，如 moakt）。
    /// 按是否跟随重定向分两份缓存，配置版本变更时统一重建。
    /// </summary>
    private static HttpClient GetRawClient(bool followRedirect)
    {
        lock (_lock)
        {
            var version = Config.Version;
            if (_cachedRawVersion != version)
            {
                _cachedRawRedirect?.Dispose();
                _cachedRawNoRedirect?.Dispose();
                _cachedRawRedirect = null;
                _cachedRawNoRedirect = null;
                _cachedRawVersion = version;
            }

            if (followRedirect)
                return _cachedRawRedirect ??= BuildClient(true, useCookies: false);
            return _cachedRawNoRedirect ??= BuildClient(false, useCookies: false);
        }
    }

    /// <summary>
    /// 裸 GET 请求：使用禁用 Cookie 容器的客户端，响应的 Set-Cookie 头原样保留在
    /// <see cref="HttpResult.SetCookies"/> 中，供 provider 手动合并 Cookie 头。
    /// </summary>
    public static HttpResult RawGet(string url, IDictionary<string, string>? headers = null,
        bool followRedirect = true, int? timeout = null)
        => Send(HttpMethod.Get, url, headers, null, null, timeout, followRedirect, raw: true);

    /// <summary>
    /// 裸 POST 请求：使用禁用 Cookie 容器的客户端。followRedirect=false 时可读取 302 响应的
    /// Set-Cookie（moakt 创建邮箱依赖此行为捕获 tm_session）。
    /// </summary>
    public static HttpResult RawPost(string url, string? body = null, string? contentType = null,
        IDictionary<string, string>? headers = null, bool followRedirect = true, int? timeout = null)
        => Send(HttpMethod.Post, url, headers, body, contentType, timeout, followRedirect, raw: true);

    /// <summary>按配置构建 HttpClient（followRedirect 控制是否自动跟随重定向；useCookies 控制内部 Cookie 容器）</summary>
    private static HttpClient BuildClient(bool followRedirect, bool useCookies = true)
    {
        var cfg = Config.Get();
        // SocketsHttpHandler：显式管理连接池，PooledConnectionLifetime 防止长驻连接绕过 DNS 变更，
        // PooledConnectionIdleTimeout 回收空闲连接，MaxConnectionsPerServer 提升并发收信吞吐。
        var handler = new SocketsHttpHandler
        {
            AllowAutoRedirect = followRedirect,
            UseCookies = useCookies,
            AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate | DecompressionMethods.Brotli,
            PooledConnectionLifetime = TimeSpan.FromMinutes(5),
            PooledConnectionIdleTimeout = TimeSpan.FromMinutes(2),
            MaxConnectionsPerServer = 64,
        };

        if (!string.IsNullOrWhiteSpace(cfg.Proxy))
        {
            handler.Proxy = new WebProxy(cfg.Proxy);
            handler.UseProxy = true;
        }

        if (cfg.Insecure)
            handler.SslOptions.RemoteCertificateValidationCallback =
                (RemoteCertificateValidationCallback)((_, _, _, _) => true);

        _cachedTimeout = cfg.Timeout;
        return new HttpClient(handler)
        {
            Timeout = TimeSpan.FromSeconds(Math.Max(1, cfg.Timeout)),
        };
    }

    /// <summary>发起 GET 请求（自动注入伪造来源 IP 请求头与全局自定义头）</summary>
    public static HttpResult Get(string url, IDictionary<string, string>? headers = null, int? timeout = null)
        => Send(HttpMethod.Get, url, headers, null, null, timeout);

    /// <summary>发起 POST 请求，body 为原始字符串</summary>
    public static HttpResult Post(string url, string? body = null, string? contentType = null,
        IDictionary<string, string>? headers = null, int? timeout = null)
        => Send(HttpMethod.Post, url, headers, body, contentType, timeout);

    /// <summary>发起不跟随重定向的 POST 请求（供需要读取 302 Location 头的渠道使用）</summary>
    public static HttpResult PostNoRedirect(string url, string? body = null, string? contentType = null,
        IDictionary<string, string>? headers = null, int? timeout = null)
        => Send(HttpMethod.Post, url, headers, body, contentType, timeout, followRedirect: false);

    /// <summary>通用请求实现（同步阻塞，provider 逻辑为同步风格）</summary>
    public static HttpResult Send(HttpMethod method, string url, IDictionary<string, string>? headers,
        string? body, string? contentType, int? timeout, bool followRedirect = true, bool raw = false)
    {
        var client = raw
            ? GetRawClient(followRedirect)
            : (followRedirect ? GetClient() : GetClientNoRedirect());
        using var req = new HttpRequestMessage(method, url);

        // 伪造来源 IP 头
        var ip = RandomIpv4();
        req.Headers.TryAddWithoutValidation("X-Forwarded-For", ip);
        req.Headers.TryAddWithoutValidation("X-Real-IP", ip);
        req.Headers.TryAddWithoutValidation("X-Originating-IP", ip);

        // 全局自定义头
        var cfg = Config.Get();
        if (cfg.Headers is not null)
            foreach (var kv in cfg.Headers)
                req.Headers.TryAddWithoutValidation(kv.Key, kv.Value);

        // 请求级头
        if (headers is not null)
            foreach (var kv in headers)
                req.Headers.TryAddWithoutValidation(kv.Key, kv.Value);

        if (body is not null)
        {
            var content = new StringContent(body, Encoding.UTF8);
            if (!string.IsNullOrEmpty(contentType))
            {
                content.Headers.Remove("Content-Type");
                content.Headers.TryAddWithoutValidation("Content-Type", contentType);
            }
            req.Content = content;
        }

        var effectiveTimeout = timeout ?? _cachedTimeout;
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(Math.Max(1, effectiveTimeout)));

        HttpResponseMessage resp;
        try
        {
            resp = client.Send(req, HttpCompletionOption.ResponseContentRead, cts.Token);
        }
        catch (OperationCanceledException)
        {
            throw new TimeoutException($"request timed out: {url}");
        }

        using (resp)
        {
            var text = resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            var result = new HttpResult
            {
                StatusCode = (int)resp.StatusCode,
                Body = text,
            };
            foreach (var h in resp.Headers)
            {
                if (string.Equals(h.Key, "Set-Cookie", StringComparison.OrdinalIgnoreCase))
                {
                    foreach (var v in h.Value) result.SetCookies.Add(v);
                }
                else
                {
                    result.Headers[h.Key] = string.Join(", ", h.Value);
                }
            }
            return result;
        }
    }
}
