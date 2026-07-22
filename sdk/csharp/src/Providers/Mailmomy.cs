using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Mailmomy 及其域名变体渠道（mailmomy.com）。
/// 免注册、无鉴权的纯 GET JSON API，Token 即邮箱地址本身。
/// </summary>
public static class Mailmomy
{
    private const string BaseUrl = "https://mailmomy.com";
    private const string DefaultDomain = "mailmomy.com";
    private static readonly Random Rand = new();
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    };

    private static string RandomLocal(int n)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder();
        for (var i = 0; i < n; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string PickDomain()
    {
        try
        {
            var resp = Http.Get($"{BaseUrl}/api/domains/active", Headers);
            resp.EnsureSuccess();
            if (Json.Parse(resp.Body) is JsonArray arr)
            {
                var domains = new List<string>();
                foreach (var d in arr)
                    if (d is JsonValue v && v.TryGetValue<string>(out var s) && !string.IsNullOrEmpty(s))
                        domains.Add(s);
                if (domains.Count > 0) return domains[Rand.Next(domains.Count)];
            }
        }
        catch { /* 回退默认域名 */ }
        return DefaultDomain;
    }

    /// <summary>创建邮箱。domain 为空则从活跃域名池随机取；channel 为对外标识。</summary>
    public static EmailInfo Generate(string channel = "mailmomy", string? domain = null)
    {
        var d = string.IsNullOrEmpty(domain) ? PickDomain() : domain!;
        var email = $"{RandomLocal(10)}@{d}";
        return new EmailInfo(channel, email, email);
    }

    public static List<Email> GetEmails(string email)
    {
        var addr = (email ?? "").Trim();
        if (string.IsNullOrEmpty(addr)) throw new Exception("mailmomy: 缺少邮箱地址");
        var resp = Http.Get($"{BaseUrl}/api/mail/messages?to={WebUtility.UrlEncode(addr)}&page=1&limit=20", Headers);
        resp.EnsureSuccess();
        var emails = (Json.Parse(resp.Body) as JsonObject)?["emails"] as JsonArray;
        var result = new List<Email>();
        if (emails is null) return result;
        foreach (var msg in emails)
        {
            if (msg is not JsonObject mo) continue;
            var message = Json.Str(mo, "message");
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(mo, "id"),
                ["from"] = Json.Str(mo, "from"),
                ["to"] = string.IsNullOrEmpty(Json.Str(mo, "recipient")) ? addr : Json.Str(mo, "recipient"),
                ["subject"] = Json.Str(mo, "subject"),
                ["text"] = string.IsNullOrEmpty(Json.Str(mo, "bodyText")) ? message : Json.Str(mo, "bodyText"),
                ["html"] = message,
                ["date"] = Json.Str(mo, "receivedAt"),
            }, addr));
        }
        return result;
    }
}
