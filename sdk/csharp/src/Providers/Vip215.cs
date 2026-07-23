using System;
using System.Collections.Generic;
using System.Net;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// vip.215.im 渠道（http_style: websocket）。
/// 创建：GET / 取首页 cookie（yyds_homepage_bridge / yyds_homepage_device），
///        POST /api/temp-inbox 得到 address + token（token 为后续 WS 鉴权 JWT）。
/// 收信：后台 WebSocket 线程持续接收 message.new 推送并累积；GetEmails 返回累积快照。
///        推送无正文时统一合成 text/html（synthetic-v1），与 Node/Go/Rust/Python 对齐。
/// 说明：.NET 内置 ClientWebSocket 承载后台推送，无需第三方依赖。
/// </summary>
public static class Vip215
{
    private const string Channel = "vip-215";
    private const string Base = "https://vip.215.im";

    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36 Edg/148.0.0.0";
    private const string SyntheticMarker = "【tempmail-sdk|synthetic|vip-215|v1】";

    private static readonly Dictionary<string, string> HomeHeaders = new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["Cache-Control"] = "no-cache",
        ["DNT"] = "1",
        ["Pragma"] = "no-cache",
        ["Sec-CH-UA"] = "\"Chromium\";v=\"148\", \"Microsoft Edge\";v=\"148\", \"Not/A)Brand\";v=\"99\"",
        ["Sec-CH-UA-Mobile"] = "?0",
        ["Sec-CH-UA-Platform"] = "\"Windows\"",
        ["Sec-Fetch-Dest"] = "document",
        ["Sec-Fetch-Mode"] = "navigate",
        ["Sec-Fetch-Site"] = "same-origin",
        ["Sec-Fetch-User"] = "?1",
        ["Upgrade-Insecure-Requests"] = "1",
    };

    private static readonly Dictionary<string, string> ApiHeaders = new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "*/*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["Cache-Control"] = "no-cache",
        ["Content-Type"] = "application/json",
        ["DNT"] = "1",
        ["Origin"] = Base,
        ["Pragma"] = "no-cache",
        ["Referer"] = $"{Base}/",
        ["Sec-CH-UA"] = "\"Chromium\";v=\"148\", \"Microsoft Edge\";v=\"148\", \"Not/A)Brand\";v=\"99\"",
        ["Sec-CH-UA-Mobile"] = "?0",
        ["Sec-CH-UA-Platform"] = "\"Windows\"",
        ["Sec-Fetch-Dest"] = "empty",
        ["Sec-Fetch-Mode"] = "cors",
        ["Sec-Fetch-Site"] = "same-origin",
        ["X-Locale"] = "zh",
    };

    private sealed class Box
    {
        public readonly List<Email> Emails = new();
        public readonly HashSet<string> Seen = new();
        public readonly string Recipient;
        public bool Started;
        public Box(string recipient) => Recipient = recipient;
    }

    private static readonly object Lock = new();
    private static readonly Dictionary<string, Box> Boxes = new();

    private static Box GetBox(string token, string recipient)
    {
        lock (Lock)
        {
            if (!Boxes.TryGetValue(token, out var b))
            {
                b = new Box(recipient);
                Boxes[token] = b;
            }
            return b;
        }
    }

    private static string EstablishSession()
    {
        var resp = Http.Get($"{Base}/", HomeHeaders, 15);
        resp.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);
        if (!cookie.Contains("yyds_homepage_bridge=") || !cookie.Contains("yyds_homepage_device="))
            throw new Exception("vip-215: missing homepage cookies");
        return cookie;
    }

    private static string FetchWsTicket(string jwt)
    {
        var headers = new Dictionary<string, string>(ApiHeaders) { ["Authorization"] = $"Bearer {jwt}" };
        var resp = Http.Get($"{Base}/v1/auth/ws-ticket", headers, 15);
        resp.EnsureSuccess();
        var body = Json.Parse(resp.Body) as JsonObject;
        if ((body?["success"] as JsonValue)?.TryGetValue<bool>(out var ok) != true || !ok)
            throw new Exception("vip-215: ws-ticket success=false");
        var ticket = Json.Str(body?["data"] as JsonObject, "ticket");
        if (ticket.Length == 0) throw new Exception("vip-215: missing ws ticket");
        return ticket;
    }

    private static void EnsureWs(string token, string recipient)
    {
        var box = GetBox(token, recipient);
        lock (Lock)
        {
            if (box.Started) return;
            box.Started = true;
        }
        var thread = new Thread(() => WsRun(token, box)) { IsBackground = true };
        thread.Start();
        Thread.Sleep(80);
    }

    private static void WsRun(string jwt, Box box)
    {
        ClientWebSocket? ws = null;
        try
        {
            var ticket = FetchWsTicket(jwt);
            var url = new Uri($"wss://vip.215.im/v1/ws?token={Uri.EscapeDataString(ticket)}");
            ws = new ClientWebSocket();
            ws.Options.SetRequestHeader("User-Agent", Ua);
            ws.Options.SetRequestHeader("Origin", Base);
            using (var cts = new CancellationTokenSource(TimeSpan.FromSeconds(15)))
                ws.ConnectAsync(url, cts.Token).GetAwaiter().GetResult();

            var buffer = new byte[16384];
            while (ws.State == WebSocketState.Open)
            {
                var sb = new StringBuilder();
                WebSocketReceiveResult res;
                do
                {
                    res = ws.ReceiveAsync(new ArraySegment<byte>(buffer), CancellationToken.None).GetAwaiter().GetResult();
                    if (res.MessageType == WebSocketMessageType.Close) return;
                    sb.Append(Encoding.UTF8.GetString(buffer, 0, res.Count));
                } while (!res.EndOfMessage);
                HandleMessage(sb.ToString(), box);
            }
        }
        catch { /* 后台线程静默退出 */ }
        finally
        {
            if (ws is not null)
            {
                try { ws.Abort(); } catch { /* 忽略 */ }
                ws.Dispose();
            }
            lock (Lock) { box.Started = false; }
        }
    }

    private static void HandleMessage(string message, Box box)
    {
        JsonObject? msg;
        try { msg = Json.Parse(message) as JsonObject; }
        catch { return; }
        if (msg is null || Json.Str(msg, "type") != "message.new") return;
        var data = msg["data"] as JsonObject ?? new JsonObject();
        var (synText, synHtml) = BuildSyntheticBodies(box.Recipient, data);
        var raw = new Dictionary<string, object?>
        {
            ["id"] = Json.Str(data, "id"),
            ["from"] = Json.Str(data, "from"),
            ["subject"] = Json.Str(data, "subject"),
            ["date"] = Json.Str(data, "date"),
            ["to"] = box.Recipient,
            ["text"] = synText,
            ["html"] = synHtml,
        };
        var em = Normalize.NormalizeEmail(raw, box.Recipient);
        if (string.IsNullOrEmpty(em.Id)) return;
        lock (Lock)
        {
            if (!box.Seen.Add(em.Id)) return;
            box.Emails.Add(em);
        }
    }

    private static string SanitizeOneLine(string? val)
        => (val ?? "").Replace("\r\n", " ").Replace("\r", " ").Replace("\n", " ").Trim();

    private static long? SizeValue(JsonObject data)
    {
        if (data["size"] is not JsonValue sv) return null;
        if (sv.TryGetValue<long>(out var l) && l >= 0) return l;
        if (sv.TryGetValue<double>(out var d) && !double.IsNaN(d) && d >= 0) return (long)d;
        return null;
    }

    private static (string Text, string Html) BuildSyntheticBodies(string recipient, JsonObject data)
    {
        var eid = SanitizeOneLine(Json.Str(data, "id"));
        var subj = SanitizeOneLine(Json.Str(data, "subject"));
        var from = SanitizeOneLine(Json.Str(data, "from"));
        var to = SanitizeOneLine(recipient);
        var date = SanitizeOneLine(Json.Str(data, "date"));
        var size = SizeValue(data);

        var lines = new List<string>
        {
            SyntheticMarker,
            $"id: {eid}",
            $"subject: {subj}",
            $"from: {from}",
            $"to: {to}",
            $"date: {date}",
        };
        if (size is not null) lines.Add($"size: {size}");
        var text = string.Join("\n", lines);

        var pairs = new List<(string, string)>
        {
            ("id", eid), ("subject", subj), ("from", from), ("to", to), ("date", date),
        };
        var sb = new StringBuilder();
        foreach (var (k, v) in pairs)
            sb.Append($"<dt>{WebUtility.HtmlEncode(k)}</dt><dd>{WebUtility.HtmlEncode(v)}</dd>");
        if (size is not null)
            sb.Append($"<dt>size</dt><dd>{WebUtility.HtmlEncode(size.ToString())}</dd>");
        var html = "<div class=\"tempmail-sdk-synthetic\" data-tempmail-sdk-format=\"synthetic-v1\" "
                   + "data-channel=\"vip-215\">"
                   + $"<dl class=\"tempmail-sdk-meta\">{sb}</dl></div>";
        return (text, html);
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var cookie = EstablishSession();
        var headers = new Dictionary<string, string>(ApiHeaders) { ["Cookie"] = cookie };
        var resp = Http.Post($"{Base}/api/temp-inbox", "", "application/json", headers, 15);
        resp.EnsureSuccess();
        var body = Json.Parse(resp.Body) as JsonObject;
        if ((body?["success"] as JsonValue)?.TryGetValue<bool>(out var ok) != true || !ok)
            throw new Exception("vip-215: success=false");
        var data = body?["data"] as JsonObject ?? new JsonObject();
        var address = Json.Str(data, "address");
        var token = Json.Str(data, "token");
        if (address.Length == 0 || token.Length == 0) throw new Exception("vip-215: missing address or token");

        EnsureWs(token, address);
        return new EmailInfo(Channel, address, token,
            createdAt: Json.Str(data, "createdAt") is { Length: > 0 } c ? c : null);
    }

    /// <summary>
    /// 通过 HTTP 接口获取邮件列表（GET /v1/messages）。
    /// 返回完整邮件（含真实正文），相比 WebSocket 推送的合成卡片更完整。
    /// </summary>
    private static List<JsonObject> FetchMessagesViaHttp(string jwt)
    {
        var headers = new Dictionary<string, string>(ApiHeaders) { ["Authorization"] = $"Bearer {jwt}" };
        var resp = Http.Get($"{Base}/v1/messages", headers, 15);
        if (resp.StatusCode < 200 || resp.StatusCode >= 300)
            throw new Exception($"vip-215: /v1/messages HTTP {resp.StatusCode}");
        var body = Json.Parse(resp.Body) as JsonObject;
        if ((body?["success"] as JsonValue)?.TryGetValue<bool>(out var ok) != true || !ok)
            throw new Exception("vip-215: /v1/messages success=false");
        var out_ = new List<JsonObject>();
        if (body?["data"] is not JsonObject data) return out_;
        if (data["messages"] is not JsonArray msgs) return out_;
        foreach (var node in msgs)
            if (node is JsonObject row) out_.Add(row);
        return out_;
    }

    /// <summary>
    /// 通过 HTTP 详情接口获取单封邮件完整内容（GET /v1/messages/{id}）。
    /// </summary>
    private static JsonObject? FetchMessageDetail(string jwt, string id)
    {
        if (string.IsNullOrWhiteSpace(id)) return null;
        try
        {
            var headers = new Dictionary<string, string>(ApiHeaders) { ["Authorization"] = $"Bearer {jwt}" };
            var resp = Http.Get($"{Base}/v1/messages/{Uri.EscapeDataString(id)}", headers, 15);
            if (resp.StatusCode < 200 || resp.StatusCode >= 300) return null;
            var body = Json.Parse(resp.Body) as JsonObject;
            if ((body?["success"] as JsonValue)?.TryGetValue<bool>(out var ok) != true || !ok) return null;
            return body?["data"] as JsonObject;
        }
        catch { return null; }
    }

    /// <summary>判断行是否包含真实正文。</summary>
    private static bool HasRealBody(JsonObject row)
    {
        foreach (var key in new[] { "text", "body_text", "html", "body_html", "content", "body" })
        {
            if (row[key] is JsonValue jv && jv.TryGetValue<string>(out var s) && !string.IsNullOrWhiteSpace(s))
                return true;
        }
        return false;
    }

    /// <summary>提取邮件 ID。</summary>
    private static string ExtractMessageId(JsonObject row)
    {
        foreach (var key in new[] { "id", "messageId", "message_id" })
        {
            var s = Json.Str(row, key).Trim();
            if (s.Length > 0) return s;
        }
        return "";
    }

    /// <summary>
    /// 获取邮件列表（HTTP 优先，WebSocket 回退）。
    /// 1. HTTP GET /v1/messages 拉取完整邮件（含真实正文）
    /// 2. 缺正文的条目通过 GET /v1/messages/{id} 补拉详情
    /// 3. HTTP 失败时回退到 WebSocket 已累积的合成卡片
    /// </summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var tok = (token ?? "").Trim();
        if (tok.Length == 0) throw new Exception("vip-215: missing token");

        /* 保持 WebSocket 订阅（供实时通知），HTTP 失败时提供回退数据 */
        EnsureWs(tok, email);

        try
        {
            var messages = FetchMessagesViaHttp(tok);
            var result = new List<Email>();
            foreach (var row in messages)
            {
                var id = ExtractMessageId(row);
                if (!HasRealBody(row) && id.Length > 0)
                {
                    var detail = FetchMessageDetail(tok, id);
                    if (detail is not null)
                    {
                        foreach (var kv in detail)
                        {
                            if (kv.Value is null) continue;
                            row[kv.Key] = kv.Value.DeepClone();
                        }
                    }
                }
                if (row["to"] is null) row["to"] = email;
                result.Add(Normalize.NormalizeEmail(Json.ToDict(row), email));
            }
            return result;
        }
        catch
        {
            /* HTTP 失败时回退到 WebSocket 累积快照（合成卡片） */
            var box = GetBox(tok, email);
            lock (Lock) { return new List<Email>(box.Emails); }
        }
    }
}
