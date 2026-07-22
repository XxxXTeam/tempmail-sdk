using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// WebMailTemp 渠道 — https://webmailtemp.com。
/// 创建：GET /api/create 返回 email、username、ttl，并下发 session cookie。
///       token 序列化保存 username 与 cookie 头串（wmt1: + base64）。
/// 收信：GET /api/check/{username}，携带 cookie，返回 emails 数组。
/// </summary>
public static class WebMailTemp
{
    private const string BaseUrl = "https://webmailtemp.com";
    private const string TokPrefix = "wmt1:";
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0",
    };

    private static string EncodeToken(string username, string cookie)
    {
        var raw = Encoding.UTF8.GetBytes(Json.Serialize(new Dictionary<string, object?> { ["u"] = username, ["c"] = cookie }));
        return TokPrefix + Convert.ToBase64String(raw);
    }

    private static (string Username, string Cookie) DecodeToken(string token)
    {
        if (!token.StartsWith(TokPrefix, StringComparison.Ordinal)) throw new Exception("webmailtemp: invalid token");
        var o = Json.Parse(Encoding.UTF8.GetString(Convert.FromBase64String(token[TokPrefix.Length..]))) as JsonObject;
        var username = Json.Str(o, "u").Trim();
        var cookie = Json.Str(o, "c").Trim();
        if (string.IsNullOrEmpty(username) || string.IsNullOrEmpty(cookie)) throw new Exception("webmailtemp: invalid token data");
        return (username, cookie);
    }

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient) => new()
    {
        ["id"] = Json.Str(raw, "id"),
        ["from"] = Json.Str(raw, "from"),
        ["to"] = recipient,
        ["subject"] = Json.Str(raw, "subject"),
        ["text"] = Json.Str(raw, "body"),
        ["html"] = Json.Str(raw, "html"),
        ["date"] = !string.IsNullOrEmpty(Json.Str(raw, "received_at")) ? Json.Str(raw, "received_at") : Json.Str(raw, "timestamp"),
        ["attachments"] = raw["attachments"] is JsonArray a ? Json.ToRaw(a) : null,
    };

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{BaseUrl}/api/create", new Dictionary<string, string>(Headers));
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var email = Json.Str(data, "email").Trim();
        var username = Json.Str(data, "username").Trim();
        var cookieHeader = FirstCookie(resp);
        var success = (data?["success"] as JsonValue)?.GetValue<bool>() ?? false;
        if (!success || string.IsNullOrEmpty(email) || !email.Contains('@')
            || string.IsNullOrEmpty(username) || string.IsNullOrEmpty(cookieHeader))
            throw new Exception("webmailtemp: invalid create response");

        long? expires = null;
        if (data!.TryGetPropertyValue("ttl", out var ttlNode) && ttlNode is JsonValue tv
            && tv.TryGetValue<double>(out var ttl) && ttl > 0)
            expires = (long)((DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() / 1000.0 + ttl) * 1000);

        return new EmailInfo("webmailtemp", email, EncodeToken(username, cookieHeader), expires);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var address = (email ?? "").Trim();
        if (string.IsNullOrEmpty(address)) throw new Exception("webmailtemp: empty email");
        var (username, cookie) = DecodeToken(token ?? "");

        var h = new Dictionary<string, string>(Headers) { ["Cookie"] = cookie };
        var resp = Http.Get($"{BaseUrl}/api/check/{WebUtility.UrlEncode(username)}", h);
        resp.EnsureSuccess();
        var rows = (Json.Parse(resp.Body) as JsonObject)?["emails"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var r in rows)
            if (r is JsonObject row)
                result.Add(Normalize.NormalizeEmail(Flatten(row, address), address));
        return result;
    }

    // 取响应中第一条 Set-Cookie 的 name=value（对齐 Python 的 next(iter(cookies))）
    private static string FirstCookie(HttpResult resp)
    {
        if (resp.SetCookies.Count == 0) return "";
        var raw = resp.SetCookies[0];
        var semi = raw.IndexOf(';');
        return (semi < 0 ? raw : raw[..semi]).Trim();
    }
}
