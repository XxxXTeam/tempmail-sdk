using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Temp-Mail.Now 渠道 — https://temp-mail.now（基于会话 Cookie）。
/// 创建：GET /en/ 取 session cookie，POST /change_email 创建邮箱。
/// 取信：GET /fetch_emails（携带 session cookie）。
/// </summary>
public static class TempMailNow
{
    private const string BaseUrl = "https://temp-mail.now";

    private static readonly Dictionary<string, string> PageHeaders = new()
    {
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    };

    private static Dictionary<string, string> ApiHeaders(string cookie) => new()
    {
        ["Accept"] = "application/json, text/javascript, */*; q=0.01",
        ["X-Requested-With"] = "XMLHttpRequest",
        ["Referer"] = $"{BaseUrl}/en/",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        ["Cookie"] = cookie,
    };

    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{BaseUrl}/en/", PageHeaders);
        resp.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);
        if (string.IsNullOrEmpty(cookie)) throw new Exception("temp-mail-now: 无法获取 session cookie");

        var resp2 = Http.Post($"{BaseUrl}/change_email", null, null, ApiHeaders(cookie));
        resp2.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, resp2);

        var data = Json.Parse(resp2.Body);
        var address = Json.Str(data, "new_email");
        if (string.IsNullOrEmpty(address) || !address.Contains('@'))
            throw new Exception("temp-mail-now: 创建邮箱失败");
        return new EmailInfo("temp-mail-now", address, cookie);
    }

    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("temp-mail-now: session cookie 不能为空");
        var addr = (email ?? "").Trim();
        if (string.IsNullOrEmpty(addr)) throw new Exception("temp-mail-now: 邮箱地址不能为空");

        var resp = Http.Get($"{BaseUrl}/fetch_emails", ApiHeaders(token));
        resp.EnsureSuccess();
        var result = new List<Email>();
        if ((Json.Parse(resp.Body) as JsonObject)?["emails"] is not JsonArray list) return result;

        foreach (var item in list)
        {
            if (item is not JsonObject msg) continue;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(msg, "id"),
                ["from"] = FirstNonEmpty(Json.Str(msg, "from"), Json.Str(msg, "from_address"), Json.Str(msg, "sender")),
                ["to"] = string.IsNullOrEmpty(Json.Str(msg, "to")) ? addr : Json.Str(msg, "to"),
                ["subject"] = Json.Str(msg, "subject"),
                ["text"] = FirstNonEmpty(Json.Str(msg, "text"), Json.Str(msg, "body_text")),
                ["html"] = FirstNonEmpty(Json.Str(msg, "html"), Json.Str(msg, "body_html")),
                ["date"] = FirstNonEmpty(Json.Str(msg, "date"), Json.Str(msg, "received_at")),
            }, addr));
        }
        return result;
    }

    private static string FirstNonEmpty(params string[] values)
    {
        foreach (var v in values) if (!string.IsNullOrEmpty(v)) return v;
        return "";
    }
}
