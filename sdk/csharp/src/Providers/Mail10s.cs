using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>Mail10s 渠道（mail10s.com）。本地生成随机地址，GET 收信。</summary>
public static class Mail10s
{
    private const string BaseUrl = "https://mail10s.com";
    private const string Domain = "mail10s.com";
    private static readonly Random Rand = new();

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0",
    };

    private static string RandomLocal()
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder("sdk");
        for (var i = 0; i < 16; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient)
    {
        return new Dictionary<string, object?>
        {
            ["id"] = Pick(raw, "id"),
            ["from"] = Pick(raw, "sender"),
            ["to"] = recipient,
            ["subject"] = Pick(raw, "subject"),
            ["text"] = Pick(raw, "body_text"),
            ["html"] = Pick(raw, "body_html"),
            ["date"] = Pick(raw, "received_at"),
            ["attachments"] = Json.ToRaw(raw["attachments"] as JsonArray),
        };
    }

    /// <summary>创建临时邮箱，本地拼接，token = 邮箱地址</summary>
    public static EmailInfo Generate()
    {
        var email = $"{RandomLocal()}@{Domain}";
        return new EmailInfo("mail10s", email, email);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("mail10s: empty email");
        var resp = Http.Get($"{BaseUrl}/api/emails/{WebUtility.UrlEncode(address)}/inbox", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var messages = (data?["data"] as JsonObject)?["messages"] as JsonArray;
        var result = new List<Email>();
        if (messages is null) return result;
        foreach (var row in messages)
            if (row is JsonObject ro) result.Add(Normalize.NormalizeEmail(Flatten(ro, address), address));
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
