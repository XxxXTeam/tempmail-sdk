using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// ExpressInboxHub 渠道 — https://expressinboxhub.com
/// Fake_trash_mail 模式，需 CSRF token + session cookies。域名 mail42.shop。
/// 创建：GET / 取 meta CSRF + cookie → POST /messages（body _token）取 mailbox。
/// 收信：POST /messages 取 messages 数组。
/// token 存 JSON {"csrfToken","cookies"}。
/// </summary>
public static class ExpressInboxHub
{
    private const string Base = "https://expressinboxhub.com";

    private static readonly Regex CsrfRe = new(
        "<meta\\s+name=[\"']csrf-token[\"']\\s+content=[\"']([^\"']+)[\"']", RegexOptions.Compiled);

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Content-Type"] = "application/json",
        ["Origin"] = Base,
        ["Referer"] = $"{Base}/",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static (string csrf, string cookie) InitSession()
    {
        var resp = Http.Get(Base, new Dictionary<string, string> { ["User-Agent"] = Headers()["User-Agent"] });
        resp.EnsureSuccess();
        var m = CsrfRe.Match(resp.Body);
        if (!m.Success) throw new Exception("expressinboxhub: 无法提取 CSRF token");
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);
        if (string.IsNullOrEmpty(cookie)) throw new Exception("expressinboxhub: 未获取到 session cookies");
        return (m.Groups[1].Value, cookie);
    }

    private static JsonObject PostMessages(string csrf, string cookie)
    {
        var headers = Headers();
        headers["X-CSRF-TOKEN"] = csrf;
        headers["Cookie"] = cookie;
        var payload = Json.Serialize(new Dictionary<string, object?> { ["_token"] = csrf });
        var resp = Http.Post($"{Base}/messages", payload, "application/json", headers);
        resp.EnsureSuccess();
        return Json.Parse(resp.Body) as JsonObject ?? throw new Exception("expressinboxhub: 响应格式异常");
    }

    /// <summary>创建 expressinboxhub 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var (csrf, cookie) = InitSession();
        var data = PostMessages(csrf, cookie);
        var mailbox = Json.Str(data, "mailbox").Trim();
        if (mailbox.Length == 0) throw new Exception("expressinboxhub: 响应中缺少 mailbox 字段");

        var token = Json.Serialize(new Dictionary<string, object?> { ["csrfToken"] = csrf, ["cookies"] = cookie });
        return new EmailInfo("expressinboxhub", mailbox, token);
    }

    /// <summary>获取 expressinboxhub 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("expressinboxhub: 缺少 token");
        var session = Json.Parse(token) as JsonObject ?? throw new Exception("expressinboxhub: 无效 token");
        var csrf = Json.Str(session, "csrfToken");
        var cookie = Json.Str(session, "cookies");

        var data = PostMessages(csrf, cookie);
        var result = new List<Email>();
        if (data["messages"] is not JsonArray messages) return result;

        foreach (var node in messages)
        {
            if (node is not JsonObject raw) continue;
            var flat = Json.ToDict(raw);
            // receivedAt → date，content → html
            if (flat.TryGetValue("receivedAt", out var ra) && !flat.ContainsKey("date")) flat["date"] = ra;
            if (flat.TryGetValue("content", out var c) && !flat.ContainsKey("html")) flat["html"] = c;
            result.Add(Normalize.NormalizeEmail(flat, email));
        }
        return result;
    }
}
