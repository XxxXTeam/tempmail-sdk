using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>InboxKitten 渠道（inboxkitten.com/api/v1/mail）。</summary>
public static class InboxKitten
{
    private const string ApiBase = "https://inboxkitten.com/api/v1/mail";
    private const string Domain = "inboxkitten.com";
    private static readonly Random Rand = new();
    private static readonly Dictionary<string, string> HeadersJson = new() { ["Accept"] = "application/json" };
    private static readonly Dictionary<string, string> HeadersHtml = new() { ["Accept"] = "text/html,*/*" };

    private static string RandomLocal()
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder("sdk");
        for (var i = 0; i < 16; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    public static EmailInfo Generate() => new("inboxkitten", $"{RandomLocal()}@{Domain}");

    public static List<Email> GetEmails(string email)
    {
        email = (email ?? "").Trim();
        var local = email.Split('@')[0];
        if (string.IsNullOrEmpty(local)) throw new Exception("inboxkitten: empty email");

        var resp = Http.Get($"{ApiBase}/list?recipient={WebUtility.UrlEncode(local)}", HeadersJson);
        resp.EnsureSuccess();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var item in rows)
            if (item is JsonObject row)
                result.Add(Normalize.NormalizeEmail(DetailRaw(row, email), email));
        return result;
    }

    private static Dictionary<string, object?> DetailRaw(JsonObject row, string recipient)
    {
        var storage = row["storage"] as JsonObject;
        var region = (storage?["region"]?.GetValue<string>() ?? "").Trim();
        var key = (storage?["key"]?.GetValue<string>() ?? "").Trim();
        var message = row["message"] as JsonObject;
        var headers = message?["headers"] as JsonObject;
        var envelope = row["envelope"] as JsonObject;

        var raw = new Dictionary<string, object?>
        {
            ["id"] = key,
            ["messageId"] = key,
            ["from"] = FirstNonEmpty(headers?["from"]?.GetValue<string>(), envelope?["sender"]?.GetValue<string>()),
            ["to"] = FirstNonEmpty(row["recipient"]?.GetValue<string>(), recipient),
            ["subject"] = headers?["subject"]?.GetValue<string>() ?? "",
        };
        if (string.IsNullOrEmpty(region) || string.IsNullOrEmpty(key)) return raw;

        try
        {
            var metaResp = Http.Get($"{ApiBase}/getKey?region={WebUtility.UrlEncode(region)}&key={WebUtility.UrlEncode(key)}", HeadersJson);
            metaResp.EnsureSuccess();
            var meta = Json.Parse(metaResp.Body) as JsonObject;
            var htmlResp = Http.Get($"{ApiBase}/getHtml?region={WebUtility.UrlEncode(region)}&key={WebUtility.UrlEncode(key)}", HeadersHtml);
            htmlResp.EnsureSuccess();
            var html = htmlResp.Body;
            raw["from"] = FirstNonEmpty(meta?["name"]?.GetValue<string>(), raw["from"] as string);
            raw["subject"] = FirstNonEmpty(meta?["subject"]?.GetValue<string>(), raw["subject"] as string);
            raw["text"] = Normalize.HtmlToText(html);
            raw["html"] = html;
        }
        catch { /* 保留摘要 */ }
        return raw;
    }

    private static string FirstNonEmpty(params string?[] values)
    {
        foreach (var v in values) if (!string.IsNullOrWhiteSpace(v)) return v!;
        return "";
    }
}

/// <summary>1SecMail 渠道（tmaily.com，会话 Cookie 鉴权）。</summary>
public static class OneSecMail
{
    private const string BaseUrl = "https://tmaily.com/";
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0",
    };

    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{BaseUrl}generate", Headers);
        resp.EnsureSuccess();
        var cookie = ExtractCookie(resp);
        if (string.IsNullOrEmpty(cookie)) throw new Exception("1sec-mail: 未获取到会话 Cookie");
        var email = Json.Str(Json.Parse(resp.Body), "address").Trim();
        if (!email.Contains('@')) throw new Exception("1sec-mail: 无效的邮箱响应");
        return new EmailInfo("1sec-mail", email, cookie);
    }

    private static string ExtractCookie(HttpResult resp)
    {
        foreach (var c in resp.SetCookies)
        {
            var idx = c.IndexOf("TMaily_sid=", StringComparison.Ordinal);
            if (idx < 0) continue;
            var seg = c[idx..];
            var semi = seg.IndexOf(';');
            return semi < 0 ? seg : seg[..semi];
        }
        return "";
    }

    public static List<Email> GetEmails(string? token, string email)
    {
        var address = (email ?? "").Trim();
        if (string.IsNullOrEmpty(address)) throw new Exception("1sec-mail: 邮箱地址为空");
        var headers = new Dictionary<string, string>(Headers) { ["Cookie"] = token ?? "" };
        var resp = Http.Get($"{BaseUrl}emails?address={WebUtility.UrlEncode(address)}", headers);
        resp.EnsureSuccess();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var item in rows)
            if (item is JsonObject) result.Add(Normalize.NormalizeEmail(Json.ToDict(item), address));
        return result;
    }
}
