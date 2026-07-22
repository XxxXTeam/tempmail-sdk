using System;

namespace XxxXTeam.TempMail;

/// <summary>
/// SDK 全局配置：代理、超时、TLS、遥测等，作用于所有 HTTP 请求。
///
/// 支持环境变量自动读取（优先级低于代码显式设置）：
///   TEMPMAIL_PROXY               - 代理 URL（http/https/socks5）
///   TEMPMAIL_TIMEOUT             - 超时秒数
///   TEMPMAIL_INSECURE            - "1"/"true" 跳过 TLS 验证
///   DROPMAIL_AUTH_TOKEN          - DropMail GraphQL af_ 令牌
///   APIHZ_ID / APIHZ_KEY         - apihz（接口盒子）调用凭据
///   TEMPMAIL_TELEMETRY_ENABLED   - false/0/no 关闭匿名用量上报
///   TEMPMAIL_TELEMETRY_URL       - 自定义上报端点
/// </summary>
public sealed class SdkConfig
{
    /// <summary>代理 URL，支持 http/https/socks5，如 "http://127.0.0.1:7890"</summary>
    public string? Proxy { get; set; }

    /// <summary>全局默认超时秒数</summary>
    public int Timeout { get; set; } = 15;

    /// <summary>跳过 TLS 证书验证（调试用）</summary>
    public bool Insecure { get; set; }

    /// <summary>自定义请求头，会合并到每个请求中</summary>
    public Dictionary<string, string>? Headers { get; set; }

    /// <summary>DropMail GraphQL 路径中的 af_ 令牌；空则自动申请</summary>
    public string? DropmailAuthToken { get; set; }

    /// <summary>apihz 调用 id；空则使用官方公共账号</summary>
    public string? ApihzId { get; set; }

    /// <summary>apihz 调用 key；空则使用官方公共账号</summary>
    public string? ApihzKey { get; set; }

    /// <summary>null 表示默认开启匿名上报；false 关闭；true 显式开启</summary>
    public bool? TelemetryEnabled { get; set; }

    /// <summary>非空时覆盖默认上报 URL</summary>
    public string? TelemetryUrl { get; set; }
}

/// <summary>
/// 全局配置管理器。
/// 设置配置后自动递增版本号，使共享 HttpClient 失效并按新配置重建。
/// </summary>
public static class Config
{
    private static readonly object _lock = new();
    // volatile 引用/计数：读取路径（Http 热路径每请求多次调用 Get/Version）无需加锁，
    // 引用赋值与 int 读写本身原子，写入侧仍在 lock 内保证版本递增与替换的顺序一致性。
    private static volatile SdkConfig _global = LoadEnvConfig();
    private static volatile int _version;

    /// <summary>从环境变量读取默认配置</summary>
    private static SdkConfig LoadEnvConfig()
    {
        var cfg = new SdkConfig();

        var proxy = Environment.GetEnvironmentVariable("TEMPMAIL_PROXY");
        if (!string.IsNullOrWhiteSpace(proxy)) cfg.Proxy = proxy;

        var timeoutStr = Environment.GetEnvironmentVariable("TEMPMAIL_TIMEOUT");
        if (int.TryParse(timeoutStr, out var t) && t > 0) cfg.Timeout = t;

        var insecure = Environment.GetEnvironmentVariable("TEMPMAIL_INSECURE") ?? "";
        cfg.Insecure = insecure is "1" or "true" or "True" or "TRUE";

        var dmTok = Environment.GetEnvironmentVariable("DROPMAIL_AUTH_TOKEN")
                    ?? Environment.GetEnvironmentVariable("DROPMAIL_API_TOKEN");
        if (!string.IsNullOrWhiteSpace(dmTok)) cfg.DropmailAuthToken = dmTok.Trim();

        var apihzId = Environment.GetEnvironmentVariable("APIHZ_ID");
        if (!string.IsNullOrWhiteSpace(apihzId)) cfg.ApihzId = apihzId.Trim();
        var apihzKey = Environment.GetEnvironmentVariable("APIHZ_KEY");
        if (!string.IsNullOrWhiteSpace(apihzKey)) cfg.ApihzKey = apihzKey.Trim();

        var te = (Environment.GetEnvironmentVariable("TEMPMAIL_TELEMETRY_ENABLED") ?? "").Trim().ToLowerInvariant();
        if (te is "false" or "0" or "no") cfg.TelemetryEnabled = false;
        else if (te is "true" or "1" or "yes") cfg.TelemetryEnabled = true;

        var tu = Environment.GetEnvironmentVariable("TEMPMAIL_TELEMETRY_URL");
        if (!string.IsNullOrWhiteSpace(tu)) cfg.TelemetryUrl = tu.Trim();

        return cfg;
    }

    /// <summary>设置 SDK 全局配置，触发共享 HttpClient 失效重建</summary>
    public static void Set(SdkConfig config)
    {
        lock (_lock)
        {
            _global = config ?? new SdkConfig();
            _version++;
        }
    }

    /// <summary>获取当前 SDK 全局配置（volatile 无锁读取）</summary>
    public static SdkConfig Get() => _global;

    /// <summary>获取当前配置版本号，用于缓存失效判断（volatile 无锁读取）</summary>
    public static int Version => _version;
}
