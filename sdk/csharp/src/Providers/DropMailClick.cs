using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// dropmail-click 渠道（dropmail.click）。免注册无鉴权，Token 即邮箱地址本身。
/// </summary>
public static class DropMailClick
{
    private const string BaseUrl = "https://dropmail.click";

    private static string Ua()
    {
        var cfg = Config.Get();
        if (cfg.Headers is not null && cfg.Headers.TryGetValue("User-Agent", out var ua) && !string.IsNullOrEmpty(ua))
            return ua;
        return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";
    }

    private static Dictionary<string, string> BuildHeaders() => new()
    {
        ["User-Agent"] = Ua(),
        ["Accept"] = "application/json",
    };

    private static string First(JsonObject msg, params string[] keys)
    {
        foreach (var key in keys)
        {
            if (msg[key] is JsonNode n)
            {
                var s = Json.NodeToString(n);
                if (!string.IsNullOrEmpty(s)) return s;
            }
        }
        return "";
    }

    /// <summary>创建临时邮箱：POST /api/v1/public/mailbox；token = 邮箱地址</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Post($"{BaseUrl}/api/v1/public/mailbox", "", null, BuildHeaders());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null) throw new Exception("dropmail-click: 无效响应");
        var address = data["address"]?.GetValue<string>();
        if (string.IsNullOrEmpty(address)) throw new Exception("dropmail-click: 无效响应，缺少 address");

        long? expiresAt = null;
        if (data["expires_at"] is JsonValue ev)
        {
            if (ev.TryGetValue<long>(out var l)) expiresAt = l;
            else if (ev.TryGetValue<double>(out var d)) expiresAt = (long)d;
        }
        return new EmailInfo("dropmail-click", address, address, expiresAt);
    }

    /// <summary>获取邮件列表：GET /api/v1/public/mailbox/{email}</summary>
    public static List<Email> GetEmails(string email)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("dropmail-click: 缺少邮箱地址");

        var resp = Http.Get($"{BaseUrl}/api/v1/public/mailbox/{WebUtility.UrlEncode(addr)}", BuildHeaders());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data is null) return result;
        if (data["messages"] is not JsonArray messages) return result;

        foreach (var m in messages)
        {
            if (m is not JsonObject msg) continue;
            var row = new Dictionary<string, object?>
            {
                ["id"] = First(msg, "id"),
                ["from"] = First(msg, "from"),
                ["to"] = FirstOr(First(msg, "address"), addr),
                ["subject"] = First(msg, "subject"),
                ["text"] = First(msg, "text"),
                ["html"] = First(msg, "html"),
                ["date"] = First(msg, "received_at", "date"),
            };
            result.Add(Normalize.NormalizeEmail(row, addr));
        }
        return result;
    }

    private static string FirstOr(string value, string fallback) => value.Length > 0 ? value : fallback;
}
