using System;
using System.Collections.Generic;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Socket.IO 临时邮箱共享实现（mjj.cm / linshi.co 等使用相同 Socket.IO 协议的站点）。
/// 使用 .NET 内置 ClientWebSocket 实现 Engine.IO/Socket.IO 握手，
/// 后台线程持续接收 "mail" 事件累积到邮箱状态，get_emails 返回累积结果。
/// </summary>
public static class SocketIoMail
{
    private const int ConnectTimeoutSec = 15;
    private static readonly int[] SocketIoVersions = { 4, 3 };
    private const int HandshakeWaitMs = 1000;
    private const int InitialSyncWaitMs = 80;

    private sealed class BoxState
    {
        public string Email = "";
        public string Channel = "";
        public readonly List<Email> Emails = new();
        public readonly HashSet<string> SeenIds = new(StringComparer.Ordinal);
        public ClientWebSocket? Ws;
        public readonly object Lock = new();
    }

    private sealed class Provider
    {
        private readonly string _channel;
        private readonly string _defaultHost;
        private readonly Dictionary<string, BoxState> _mailboxes = new(StringComparer.Ordinal);
        private readonly object _lock = new();

        public Provider(string channel, string defaultHost)
        {
            _channel = channel;
            _defaultHost = defaultHost;
        }

        private static string SocketUrl(string host, int eio)
            => $"wss://{host}/socket.io/?EIO={eio}&transport=websocket";

        // 接收一条完整的文本帧
        private static string ReceiveText(ClientWebSocket ws)
        {
            var buffer = new byte[8192];
            var sb = new StringBuilder();
            while (true)
            {
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(ConnectTimeoutSec));
                var seg = new ArraySegment<byte>(buffer);
                var res = ws.ReceiveAsync(seg, cts.Token).GetAwaiter().GetResult();
                if (res.MessageType == WebSocketMessageType.Close)
                    throw new Exception("socketio: connection closed");
                sb.Append(Encoding.UTF8.GetString(buffer, 0, res.Count));
                if (res.EndOfMessage) break;
            }
            return sb.ToString();
        }

        private static void SendRaw(ClientWebSocket ws, string data)
        {
            var bytes = Encoding.UTF8.GetBytes(data);
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(ConnectTimeoutSec));
            ws.SendAsync(new ArraySegment<byte>(bytes), WebSocketMessageType.Text, true, cts.Token)
                .GetAwaiter().GetResult();
        }

        private static void SendEvent(ClientWebSocket ws, string ev, JsonNode? payload)
        {
            var arr = new JsonArray { ev, payload };
            SendRaw(ws, "42" + arr.ToJsonString());
        }

        // 解析 Socket.IO 事件包 "42[event, payload]"
        private static (string Event, JsonNode? Payload)? ParseEventPacket(string packet)
        {
            if (!packet.StartsWith("42", StringComparison.Ordinal)) return null;
            try
            {
                if (Json.Parse(packet[2..]) is not JsonArray decoded || decoded.Count < 2) return null;
                if (decoded[0] is not JsonValue v || !v.TryGetValue<string>(out var ev)) return null;
                return (ev, decoded[1]);
            }
            catch { return null; }
        }

        private ClientWebSocket ConnectSocket(string host)
        {
            Exception? lastErr = null;
            foreach (var version in SocketIoVersions)
            {
                var url = SocketUrl(host, version);
                ClientWebSocket? ws = null;
                try
                {
                    ws = new ClientWebSocket();
                    ws.Options.SetRequestHeader("User-Agent",
                        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
                    ws.Options.SetRequestHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6");
                    ws.Options.SetRequestHeader("Cache-Control", "no-cache");
                    ws.Options.SetRequestHeader("Pragma", "no-cache");
                    ws.Options.SetRequestHeader("Origin", $"https://{host}");
                    using (var cts = new CancellationTokenSource(TimeSpan.FromSeconds(ConnectTimeoutSec)))
                        ws.ConnectAsync(new Uri(url), cts.Token).GetAwaiter().GetResult();

                    // 等待 Engine.IO open 包并发送 Socket.IO connect
                    var sentConnect = false;
                    for (var i = 0; i < 10; i++)
                    {
                        var msg = ReceiveText(ws);
                        if (msg == "2") { SendRaw(ws, "3"); continue; }
                        if (!sentConnect)
                        {
                            if (!msg.StartsWith("0", StringComparison.Ordinal))
                                throw new Exception($"{_channel}: unexpected open packet for EIO={version}");
                            sentConnect = true;
                            SendRaw(ws, "40");
                            Thread.Sleep(HandshakeWaitMs);
                            return ws;
                        }
                        if (msg.StartsWith("40", StringComparison.Ordinal)) return ws;
                    }
                    ws.Dispose();
                }
                catch (Exception e)
                {
                    lastErr = e;
                    try { ws?.Dispose(); } catch { }
                }
            }
            throw lastErr ?? new Exception($"{_channel}: websocket handshake failed");
        }

        private string RequestShortid(string host)
        {
            var ws = ConnectSocket(host);
            try
            {
                SendEvent(ws, "request shortid", JsonValue.Create(true));
                for (var i = 0; i < 50; i++)
                {
                    var msg = ReceiveText(ws);
                    if (msg == "2") { SendRaw(ws, "3"); continue; }
                    var parsed = ParseEventPacket(msg);
                    if (parsed is { } p && p.Event == "shortid"
                        && p.Payload is JsonValue v && v.TryGetValue<string>(out var sid))
                        return sid;
                }
                throw new Exception($"{_channel}: 等待 shortid 超时");
            }
            finally { try { ws.Abort(); ws.Dispose(); } catch { } }
        }

        public EmailInfo GenerateEmail()
        {
            var host = _defaultHost;
            var shortid = RequestShortid(host);
            var emailAddr = $"{shortid}@{host}";
            EnsureMailbox(emailAddr);
            return new EmailInfo(_channel, emailAddr);
        }

        private BoxState GetState(string email)
        {
            var key = email.Trim().ToLowerInvariant();
            lock (_lock)
            {
                if (!_mailboxes.TryGetValue(key, out var st))
                {
                    st = new BoxState { Email = email, Channel = _channel };
                    _mailboxes[key] = st;
                }
                return st;
            }
        }

        private void EnsureMailbox(string email)
        {
            var st = GetState(email);
            lock (st.Lock) { if (st.Ws is not null) return; }

            var at = email.IndexOf('@');
            if (at <= 0) throw new Exception($"{_channel}: invalid email address");
            var local = email[..at];
            var host = email[(at + 1)..];
            if (host.Length == 0) host = _defaultHost;

            var ws = ConnectSocket(host);
            SendEvent(ws, "set shortid", JsonValue.Create(local));
            lock (st.Lock) { st.Ws = ws; }

            _ = Task.Run(() =>
            {
                try
                {
                    while (true)
                    {
                        var msg = ReceiveText(ws);
                        if (msg == "2") { SendRaw(ws, "3"); continue; }
                        var parsed = ParseEventPacket(msg);
                        if (parsed is not { } p || p.Event != "mail" || p.Payload is not JsonObject mo) continue;
                        var normalized = Normalize.NormalizeEmail(FlattenMail(mo, email), email);
                        lock (st.Lock)
                        {
                            if (!string.IsNullOrEmpty(normalized.Id) && st.SeenIds.Add(normalized.Id))
                                st.Emails.Add(normalized);
                        }
                    }
                }
                catch
                {
                    lock (st.Lock) { if (ReferenceEquals(st.Ws, ws)) st.Ws = null; }
                    try { ws.Abort(); ws.Dispose(); } catch { }
                }
            });

            Thread.Sleep(InitialSyncWaitMs);
        }

        public List<Email> GetEmails(string email)
        {
            EnsureMailbox(email);
            var st = GetState(email);
            lock (st.Lock) { return new List<Email>(st.Emails); }
        }
    }

    // 展平邮件数据（兼容 headers 嵌套结构）
    private static Dictionary<string, object?> FlattenMail(JsonObject raw, string recipient)
    {
        var headers = raw["headers"] as JsonObject;

        string HeaderOrRaw(string key)
        {
            var h = headers is null ? "" : Json.Str(headers, key);
            return h.Length > 0 ? h : Json.Str(raw, key);
        }

        var msgId = Json.Str(raw, "id");
        if (msgId.Length == 0) msgId = Json.Str(raw, "messageId");
        if (msgId.Length == 0 && headers is not null) msgId = Json.Str(headers, "message-id");
        if (msgId.Length == 0 && headers is not null) msgId = Json.Str(headers, "messageId");
        if (msgId.Length == 0)
            msgId = $"{HeaderOrRaw("from")}\n{HeaderOrRaw("subject")}\n{HeaderOrRaw("date")}\n{recipient}";

        var text = Json.Str(raw, "text");
        if (text.Length == 0) text = Json.Str(raw, "body");

        return new Dictionary<string, object?>
        {
            ["id"] = msgId,
            ["from"] = HeaderOrRaw("from"),
            ["to"] = recipient,
            ["subject"] = HeaderOrRaw("subject"),
            ["text"] = text,
            ["html"] = Json.Str(raw, "html"),
            ["date"] = HeaderOrRaw("date"),
            ["isRead"] = false,
        };
    }

    private static readonly Provider MjjCm = new("mjj-cm", "mjj.cm");
    private static readonly Provider LinshiCo = new("linshi-co", "linshi.co");

    /// <summary>创建 mjj-cm 临时邮箱</summary>
    public static EmailInfo GenerateMjjCm() => MjjCm.GenerateEmail();

    /// <summary>获取 mjj-cm 邮件列表</summary>
    public static List<Email> GetEmailsMjjCm(string email) => MjjCm.GetEmails(email);

    /// <summary>创建 linshi-co 临时邮箱</summary>
    public static EmailInfo GenerateLinshiCo() => LinshiCo.GenerateEmail();

    /// <summary>获取 linshi-co 邮件列表</summary>
    public static List<Email> GetEmailsLinshiCo(string email) => LinshiCo.GetEmails(email);
}
