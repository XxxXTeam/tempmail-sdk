using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>Mail123 渠道（mail123.fr/api/v1）。GET JSON API，逐封拉取详情。</summary>
public static class Mail123
{
    private const string ApiBase = "https://mail123.fr/api/v1";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0",
    };

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient)
    {
        var outDict = Json.ToDict(raw);
        outDict["id"] = Pick(raw, "id");
        outDict["to"] = PickOr(raw, recipient, "to");
        outDict["text"] = PickOr(raw, "", "text", "preview");
        outDict["html"] = Pick(raw, "html");
        if (raw["is_unread"] is JsonValue uv && uv.TryGetValue<bool>(out var unread))
            outDict["isRead"] = !unread;
        return outDict;
    }

    /// <summary>创建临时邮箱：GET /mailbox/new，token = 邮箱地址</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{ApiBase}/mailbox/new", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null) throw new Exception("mail123: invalid mailbox response");
        var email = (data["address"]?.GetValue<string>() ?? "").Trim();
        if (email.Length == 0 || !email.Contains('@')) throw new Exception("mail123: invalid mailbox response");

        long? expiresAt = null;
        if (data["expires_in_days"] is JsonValue dv && dv.TryGetValue<double>(out var days) && days > 0)
            expiresAt = (long)((DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() / 1000.0 + days * 86400) * 1000);
        return new EmailInfo("mail123", email, email, expiresAt);
    }

    private static JsonObject? FetchDetail(string address, string messageId)
    {
        try
        {
            var resp = Http.Get(
                $"{ApiBase}/mailbox/{WebUtility.UrlEncode(address)}/messages/{WebUtility.UrlEncode(messageId)}",
                Headers);
            if (!resp.Ok) return null;
            var data = Json.Parse(resp.Body) as JsonObject;
            return data?["message"] as JsonObject;
        }
        catch { return null; }
    }

    /// <summary>获取邮件列表，逐封补全详情</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("mail123: empty email");
        var resp = Http.Get($"{ApiBase}/mailbox/{WebUtility.UrlEncode(address)}/messages?limit=50", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data?["messages"] is not JsonArray rows) return result;
        foreach (var item in rows)
        {
            if (item is not JsonObject msg) continue;
            var messageId = Pick(msg, "id").Trim();
            var detail = messageId.Length > 0 ? FetchDetail(address, messageId) : null;
            result.Add(Normalize.NormalizeEmail(Flatten(detail ?? msg, address), address));
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

    private static string PickOr(JsonObject o, string fallback, params string[] keys)
    {
        var v = Pick(o, keys);
        return v.Length > 0 ? v : fallback;
    }
}
