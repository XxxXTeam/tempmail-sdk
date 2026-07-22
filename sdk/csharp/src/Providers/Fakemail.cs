using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// FakeMail 渠道 — https://www.fakemail.net
/// METRONET 后端，与 disposablemail-com / minuteinbox 同结构。
/// 创建：GET / 取 session cookie + CSRF（正则 const CSRF="xxx"）→ GET /index/index?csrf_token= 建邮箱。
/// 收信：GET /index/refresh 取列表 → POST /index/email(id=) 取正文。
/// token 直接存 Cookie 头字符串。
/// </summary>
public static class Fakemail
{
    private const string BaseUrl = "https://www.fakemail.net";

    private static readonly Regex CsrfRe = new("const\\s+CSRF\\s*=\\s*\"([^\"]+)\"", RegexOptions.Compiled);

    private static Dictionary<string, string> HomeHeaders() => new()
    {
        ["Accept"] = "text/html,application/xhtml+xml",
        ["User-Agent"] = "Mozilla/5.0",
    };

    private static Dictionary<string, string> AjaxHeaders(string cookie) => new()
    {
        ["Accept"] = "application/json, text/javascript, */*; q=0.01",
        ["X-Requested-With"] = "XMLHttpRequest",
        ["Referer"] = $"{BaseUrl}/",
        ["Cookie"] = cookie,
        ["User-Agent"] = "Mozilla/5.0",
    };

    /// <summary>创建 fakemail 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{BaseUrl}/", HomeHeaders());
        resp.EnsureSuccess();
        var m = CsrfRe.Match(resp.Body);
        if (!m.Success) throw new Exception("fakemail: csrf token not found");
        var csrf = m.Groups[1].Value;
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);

        var resp2 = Http.Get($"{BaseUrl}/index/index?csrf_token={WebUtility.UrlEncode(csrf)}", AjaxHeaders(cookie));
        resp2.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, resp2);

        var data = Json.Parse(resp2.Body) as JsonObject
            ?? throw new Exception("fakemail: invalid mailbox response");
        var email = Json.Str(data, "email").Trim();
        if (email.Length == 0 || !email.Contains('@'))
            throw new Exception("fakemail: invalid mailbox response");

        return new EmailInfo("fakemail", email, cookie);
    }

    /// <summary>获取 fakemail 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var cookie = (token ?? "").Trim();
        var addr = (email ?? "").Trim();
        if (cookie.Length == 0) throw new Exception("fakemail: empty session token");
        if (addr.Length == 0) throw new Exception("fakemail: empty email");

        var resp = Http.Get($"{BaseUrl}/index/refresh", AjaxHeaders(cookie));
        resp.EnsureSuccess();

        var result = new List<Email>();
        if (Json.Parse(resp.Body) is not JsonArray rows) return result;

        foreach (var node in rows)
        {
            if (node is not JsonObject row) continue;
            var msgId = Json.Str(row, "id").Trim();
            var detail = msgId.Length > 0 ? FetchDetail(cookie, msgId) : null;

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = FirstNonEmpty(Json.Str(detail, "id"), Json.Str(row, "id")),
                ["from"] = FirstNonEmpty(Json.Str(detail, "od"), Json.Str(row, "od")),
                ["to"] = addr,
                ["subject"] = FirstNonEmpty(Json.Str(detail, "predmet"), Json.Str(row, "predmet")),
                ["text"] = "",
                ["html"] = Json.Str(detail, "telo"),
                ["date"] = Json.Str(row, "kdy"),
                ["isRead"] = Json.Str(row, "precteno") == "precteno",
            }, addr));
        }
        return result;
    }

    private static JsonObject? FetchDetail(string cookie, string messageId)
    {
        try
        {
            var resp = Http.Post($"{BaseUrl}/index/email", $"id={WebUtility.UrlEncode(messageId)}",
                "application/x-www-form-urlencoded", AjaxHeaders(cookie));
            if (!resp.Ok) return null;
            return Json.Parse(resp.Body) as JsonObject;
        }
        catch { return null; }
    }

    private static string FirstNonEmpty(params string[] values)
    {
        foreach (var v in values) if (!string.IsNullOrEmpty(v)) return v;
        return "";
    }
}
