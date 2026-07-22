using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Runtime.CompilerServices;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace XxxXTeam.TempMail;

/// <summary>
/// 嵌入式 WebUI HTTP 服务器。
/// 通过环境变量激活（默认不启动）：
///   TEMPMAIL_WEBUI=true/1/yes      - 启用 WebUI
///   TEMPMAIL_WEBUI_PORT=&lt;端口&gt;     - 指定端口，未设置则随机分配
/// </summary>
public static class WebUI
{
    private const string SdkVersion = "1.0.0";

    private static HttpListener? _listener;
    private static int _port;
    private static string _host = "127.0.0.1";
    private static readonly object _startLock = new();
    private static bool _started;

    /// <summary>活跃 SSE 连接集合，写入失败时自动移除</summary>
    private static readonly HashSet<HttpListenerResponse> _sseClients = new();
    private static readonly object _sseLock = new();

    /// <summary>JSON 序列化选项：不转义中文等非 ASCII 字符，保持日志可读</summary>
    private static readonly JsonSerializerOptions _jsonOpts = new()
    {
        Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    /// <summary>向所有活跃 SSE 连接推送一条日志</summary>
    public static void PushLog(string level, string msg, string channel = "")
    {
        lock (_sseLock) { if (_sseClients.Count == 0) return; }

        var payload = Encoding.UTF8.GetBytes(
            $"data: {JsonSerializer.Serialize(new { time = DateTime.Now.ToString("HH:mm:ss"), level, msg, channel }, _jsonOpts)}\n\n");

        List<HttpListenerResponse> dead = new();
        lock (_sseLock)
        {
            foreach (var c in _sseClients)
            {
                try { c.OutputStream.Write(payload); c.OutputStream.Flush(); }
                catch { dead.Add(c); }
            }
            foreach (var d in dead) _sseClients.Remove(d);
        }
    }

    /// <summary>启动 WebUI（幂等，多次调用只启动一次）</summary>
    public static void Start()
    {
        lock (_startLock) { if (_started) return; _started = true; }

        var hostEnv = (Environment.GetEnvironmentVariable("TEMPMAIL_WEBUI_HOST") ?? "").Trim();
        if (!string.IsNullOrEmpty(hostEnv)) _host = hostEnv;

        var portEnv = (Environment.GetEnvironmentVariable("TEMPMAIL_WEBUI_PORT") ?? "").Trim();
        int port = 0;
        if (int.TryParse(portEnv, out var p) && p > 0 && p <= 65535) port = p;

        // 未指定端口则让 OS 随机分配
        if (port == 0)
        {
            var tmp = new TcpListener(IPAddress.Parse(_host), 0);
            tmp.Start();
            port = ((IPEndPoint)tmp.LocalEndpoint).Port;
            tmp.Stop();
        }

        _listener = new HttpListener();
        _listener.Prefixes.Add($"http://{_host}:{port}/");
        _listener.Start();
        _port = port;

        Task.Run(ServeLoop);
        Console.Error.WriteLine($"[TempMail] WebUI 已启动: http://{_host}:{port}");
    }

    /// <summary>请求接收循环，后台线程运行</summary>
    private static async Task ServeLoop()
    {
        var listener = _listener!;
        while (listener.IsListening)
        {
            HttpListenerContext ctx;
            try { ctx = await listener.GetContextAsync().ConfigureAwait(false); }
            catch { break; }
            // SSE 需长连接，独立线程处理；其余请求即时返回
            _ = Task.Run(() => Dispatch(ctx));
        }
    }

    /// <summary>统一请求分发</summary>
    private static void Dispatch(HttpListenerContext ctx)
    {
        var path = ctx.Request.Url?.AbsolutePath ?? "/";
        try
        {
            if (ctx.Request.HttpMethod != "GET")
            {
                WritePlain(ctx.Response, 405, "Method Not Allowed");
                return;
            }
            switch (path)
            {
                case "/": HandleIndex(ctx.Response); break;
                case "/api/info": HandleInfo(ctx.Response); break;
                case "/api/channels": HandleChannels(ctx.Response); break;
                case "/api/logs/stream": HandleLogStream(ctx.Response); break;
                default: WritePlain(ctx.Response, 404, "Not Found"); break;
            }
        }
        catch { /* 单个请求异常不影响服务 */ }
    }

    /// <summary>GET /：返回内联前端 HTML</summary>
    private static void HandleIndex(HttpListenerResponse res)
    {
        var buf = Encoding.UTF8.GetBytes(WebUIHtml.Page);
        res.StatusCode = 200;
        res.ContentType = "text/html; charset=utf-8";
        res.OutputStream.Write(buf);
        res.Close();
    }

    /// <summary>GET /api/info：运行时与配置概览</summary>
    private static void HandleInfo(HttpListenerResponse res)
    {
        var cfg = Config.Get();
        var timeout = cfg.Timeout > 0 ? cfg.Timeout : 15;
        var info = new
        {
            language = "C#",
            version = SdkVersion,
            host = _host,
            port = _port,
            proxy = cfg.Proxy ?? "",
            timeout,
            config = new
            {
                telemetry = cfg.TelemetryEnabled ?? true,
                insecure = cfg.Insecure,
            },
        };
        WriteJson(res, info);
    }

    /// <summary>GET /api/channels：全部渠道列表</summary>
    private static void HandleChannels(HttpListenerResponse res)
    {
        var list = Registry.All
            .Select(s => new { channel = s.Channel, name = s.Name, website = s.Website })
            .ToList();
        WriteJson(res, list);
    }

    /// <summary>GET /api/logs/stream：建立 SSE 长连接</summary>
    private static void HandleLogStream(HttpListenerResponse res)
    {
        res.StatusCode = 200;
        res.ContentType = "text/event-stream; charset=utf-8";
        res.Headers.Add("Cache-Control", "no-cache");
        res.Headers.Add("Connection", "keep-alive");
        res.SendChunked = true;
        try
        {
            // 首帧注释保持连接活性
            var hello = Encoding.UTF8.GetBytes(": connected\n\n");
            res.OutputStream.Write(hello);
            res.OutputStream.Flush();
        }
        catch { res.Abort(); return; }

        lock (_sseLock) { _sseClients.Add(res); }
        // 连接由 PushLog 持续写入；写入失败时在 PushLog 中移除并关闭
    }

    /// <summary>以 JSON 响应</summary>
    private static void WriteJson(HttpListenerResponse res, object data)
    {
        var buf = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(data, _jsonOpts));
        res.StatusCode = 200;
        res.ContentType = "application/json; charset=utf-8";
        res.OutputStream.Write(buf);
        res.Close();
    }

    /// <summary>以纯文本响应指定状态码</summary>
    private static void WritePlain(HttpListenerResponse res, int status, string text)
    {
        var buf = Encoding.UTF8.GetBytes(text);
        res.StatusCode = status;
        res.ContentType = "text/plain; charset=utf-8";
        res.OutputStream.Write(buf);
        res.Close();
    }

    /// <summary>模块初始化：按环境变量决定是否自动启动 WebUI</summary>
    [ModuleInitializer]
    internal static void AutoStart()
    {
        var flag = (Environment.GetEnvironmentVariable("TEMPMAIL_WEBUI") ?? "").Trim().ToLowerInvariant();
        if (flag is "true" or "1" or "yes")
        {
            try { Start(); }
            catch (Exception ex) { Console.Error.WriteLine($"[TempMail] WebUI 启动失败: {ex.Message}"); }
        }
    }
}
