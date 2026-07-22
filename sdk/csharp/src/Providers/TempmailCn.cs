using System;
using System.Collections.Generic;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// tempmail.cn 渠道（http_style: websocket / Socket.IO）。
/// 创建：走 Socket.IO 事件协议 wss://{host}/socket.io/?EIO={4,3}&transport=websocket，
///        发送 "40" 建立命名空间，emit "request mailbox" 后等待 "mailbox" 事件返回本地名。
/// 收信：REST GET https://{host}/api/mails/{email}（可直接移植）。
/// 说明：.NET 内置 ClientWebSocket 完成握手，无需第三方依赖。
/// </summary>
public static class TempmailCn
{
    private const string Channel = "tempmail-cn";
    private const string DefaultHost = "tempmail.cn";
    private static readonly int[] SocketIoVersions = { 4, 3 };

    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";
    private const string AcceptLanguage = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6";

    private static string NormalizeHost(string? domain)
    {
        var raw = (domain ?? "").Trim();
        if (raw.Length == 0) return DefaultHost;
        var host = raw;
        var lower = host.ToLowerInvariant();
        if (lower.StartsWith("http://") || lower.StartsWith("https://"))
            host = host.Split("://", 2)[1];
        if (host.Contains('@')) host = host[(host.LastIndexOf('@') + 1)..];
        var slash = host.IndexOf('/');
        if (slash >= 0) host = host[..slash];
        host = host.Trim('.');
        return host.Length == 0 ? DefaultHost : host;
    }

    private static (string Local, string Host) SplitEmail(string email)
    {
        var trimmed = (email ?? "").Trim();
        var at = trimmed.IndexOf('@');
        if (at <= 0 || at == trimmed.Length - 1) throw new ArgumentException("tempmail-cn: invalid email address");
        return (trimmed[..at], NormalizeHost(trimmed[(at + 1)..]));
    }

    private static ClientWebSocket ConnectSocket(string host)
    {
        Exception? lastErr = null;
        foreach (var version in SocketIoVersions)
        {
            var ws = new ClientWebSocket();
            ws.Options.SetRequestHeader("User-Agent", Ua);
            ws.Options.SetRequestHeader("Origin", $"https://{host}");
            ws.Options.SetRequestHeader("Accept-Language", AcceptLanguage);
            try
            {
                var url = new Uri($"wss://{host}/socket.io/?EIO={version}&transport=websocket");
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(15));
                ws.ConnectAsync(url, cts.Token).GetAwaiter().GetResult();
                var openPacket = ReceiveText(ws, TimeSpan.FromSeconds(15));
                if (openPacket is null || !openPacket.StartsWith("0"))
                    throw new Exception($"tempmail-cn: unexpected open packet for EIO={version}");
                SendText(ws, "40");
                return ws;
            }
            catch (Exception exc)
            {
                lastErr = exc;
                try { ws.Abort(); } catch { /* 忽略 */ }
                ws.Dispose();
            }
        }
        throw new Exception(lastErr?.Message ?? "tempmail-cn: websocket handshake failed");
    }

    private static void SendText(ClientWebSocket ws, string text)
    {
        var buf = Encoding.UTF8.GetBytes(text);
        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(15));
        ws.SendAsync(new ArraySegment<byte>(buf), WebSocketMessageType.Text, true, cts.Token).GetAwaiter().GetResult();
    }

    private static string? ReceiveText(ClientWebSocket ws, TimeSpan timeout)
    {
        using var cts = new CancellationTokenSource(timeout);
        var buffer = new byte[8192];
        var sb = new StringBuilder();
        try
        {
            while (true)
            {
                var res = ws.ReceiveAsync(new ArraySegment<byte>(buffer), cts.Token).GetAwaiter().GetResult();
                if (res.MessageType == WebSocketMessageType.Close) return null;
                sb.Append(Encoding.UTF8.GetString(buffer, 0, res.Count));
                if (res.EndOfMessage) break;
            }
        }
        catch (OperationCanceledException) { return null; }
        return sb.ToString();
    }

    private static (string Event, JsonNode? Payload)? ParseEvent(string packet)
    {
        if (!packet.StartsWith("42")) return null;
        JsonNode? decoded;
        try { decoded = JsonNode.Parse(packet[2..]); }
        catch { return null; }
        if (decoded is not JsonArray arr || arr.Count == 0) return null;
        if ((arr[0] as JsonValue)?.TryGetValue<string>(out var evt) != true || evt is null) return null;
        return (evt, arr.Count > 1 ? arr[1] : null);
    }

    private static void CloseQuietly(ClientWebSocket ws)
    {
        try
        {
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
            ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "", cts.Token).GetAwaiter().GetResult();
        }
        catch { /* 忽略 */ }
        finally { ws.Dispose(); }
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate(string? domain = null)
    {
        var host = NormalizeHost(domain);
        var ws = ConnectSocket(host);
        try
        {
            SendText(ws, "42[\"request mailbox\",true]");
            var deadline = DateTime.UtcNow.AddSeconds(20);
            while (DateTime.UtcNow < deadline)
            {
                var packet = ReceiveText(ws, TimeSpan.FromSeconds(15));
                if (packet is null) break;
                if (packet == "2") { SendText(ws, "3"); continue; }
                var parsed = ParseEvent(packet);
                if (parsed is null) continue;
                var (evt, payload) = parsed.Value;
                if (evt == "mailbox" && payload is JsonValue pv && pv.TryGetValue<string>(out var local) && !string.IsNullOrWhiteSpace(local))
                    return new EmailInfo(Channel, $"{local.Trim()}@{host}");
            }
            throw new Exception("tempmail-cn: 未收到 mailbox 事件");
        }
        finally { CloseQuietly(ws); }
    }

    /// <summary>获取邮件列表（REST）</summary>
    public static List<Email> GetEmails(string email)
    {
        var (_, host) = SplitEmail(email);
        var url = $"https://{host}/api/mails/{Uri.EscapeDataString(email)}";
        var resp = Http.Get(url, new Dictionary<string, string>
        {
            ["User-Agent"] = Ua,
            ["Accept"] = "application/json",
            ["Accept-Language"] = AcceptLanguage,
            ["Referer"] = $"https://{host}/",
        });
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var mails = data?["mails"] as JsonArray;
        var outList = new List<Email>();
        if (mails is null) return outList;
        foreach (var raw in mails)
        {
            if (raw is not JsonObject ro) continue;
            var em = Normalize.NormalizeEmail(FlattenMail(ro, email), email);
            if (!string.IsNullOrEmpty(em.Id)) outList.Add(em);
        }
        return outList;
    }

    private static string ValueString(JsonNode? node)
    {
        if (node is null) return "";
        if (node is JsonValue v)
        {
            if (v.TryGetValue<string>(out var s)) return s.Trim();
            return v.ToString().Trim();
        }
        return node.ToString().Trim();
    }

    private static Dictionary<string, object?> FlattenMail(JsonObject raw, string recipient)
    {
        var headers = raw["headers"] as JsonObject ?? new JsonObject();
        var id = ValueString(raw["id"]);
        if (id.Length == 0) id = ValueString(raw["messageId"]);
        if (id.Length == 0) id = ValueString(headers["message-id"]);
        if (id.Length == 0) id = ValueString(headers["messageId"]);
        if (id.Length == 0)
            id = string.Join("\n", ValueString(headers["from"]), ValueString(headers["subject"]), ValueString(headers["date"]), recipient);

        return new Dictionary<string, object?>
        {
            ["id"] = id,
            ["from"] = ValueString(headers["from"]),
            ["to"] = recipient,
            ["subject"] = ValueString(headers["subject"]),
            ["text"] = ValueString(raw["text"]),
            ["html"] = ValueString(raw["html"]),
            ["date"] = ValueString(headers["date"]),
            ["isRead"] = false,
            ["attachments"] = new List<object?>(),
        };
    }
}
