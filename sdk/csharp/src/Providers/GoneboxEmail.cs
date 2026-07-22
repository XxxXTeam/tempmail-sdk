using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>gonebox-email 渠道（gonebox.email）。一次性临时邮箱，无需认证。</summary>
public static class GoneboxEmail
{
    private const string BaseUrl = "https://api.gonebox.email/api/v1";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        ["Accept"] = "application/json",
        ["Content-Type"] = "application/json",
    };

    /// <summary>创建临时邮箱：POST /inboxes body {"domain":"gonebox.email"}</summary>
    public static EmailInfo Generate()
    {
        var body = Json.Serialize(new Dictionary<string, object?> { ["domain"] = "gonebox.email" });
        var resp = Http.Post($"{BaseUrl}/inboxes", body, "application/json", Headers);
        resp.EnsureSuccess();
        var root = Json.Parse(resp.Body) as JsonObject;
        if (root is null || (root["success"] as JsonValue)?.GetValue<bool>() != true)
            throw new Exception("gonebox-email: 创建邮箱失败");
        if (root["data"] is not JsonObject data) throw new Exception("gonebox-email: 响应 data 格式无效");
        var address = data["address"]?.GetValue<string>() ?? "";
        if (string.IsNullOrEmpty(address)) throw new Exception("gonebox-email: 缺少邮箱地址");

        // expiresAt 为秒级时间戳，转为毫秒
        long? expiresAt = null;
        if (data["expiresAt"] is JsonValue ev)
        {
            if (ev.TryGetValue<long>(out var l)) expiresAt = l * 1000;
            else if (ev.TryGetValue<double>(out var d)) expiresAt = (long)d * 1000;
        }
        return new EmailInfo("gonebox-email", address, "", expiresAt);
    }

    /// <summary>获取邮件列表：GET /inboxes/{address}/messages</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("gonebox-email: 缺少邮箱地址");

        var headers = new Dictionary<string, string>(Headers);
        headers.Remove("Content-Type");

        var resp = Http.Get($"{BaseUrl}/inboxes/{address}/messages", headers);
        if (resp.StatusCode == 404) return new List<Email>();
        resp.EnsureSuccess();
        var root = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (root is null || (root["success"] as JsonValue)?.GetValue<bool>() != true) return result;
        if (root["data"] is not JsonObject data) return result;
        if (data["messages"] is not JsonArray messages || messages.Count == 0) return result;

        foreach (var m in messages)
        {
            if (m is not JsonObject msg) continue;
            var raw = new Dictionary<string, object?>
            {
                ["id"] = Pick(msg, "id"),
                ["from"] = Pick(msg, "from", "sender"),
                ["to"] = address,
                ["subject"] = Pick(msg, "subject"),
                ["text"] = Pick(msg, "text", "body_plain"),
                ["html"] = Pick(msg, "html", "body_html"),
                ["date"] = Pick(msg, "date", "received_at"),
            };
            result.Add(Normalize.NormalizeEmail(raw, address));
        }
        return result;
    }

    private static string Pick(JsonObject o, params string[] keys)
    {
        foreach (var k in keys)
        {
            if (o[k] is JsonNode n)
            {
                var s = Json.NodeToString(n);
                if (!string.IsNullOrEmpty(s)) return s;
            }
        }
        return "";
    }
}
