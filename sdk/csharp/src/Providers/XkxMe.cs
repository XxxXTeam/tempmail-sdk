using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// xkx.me 渠道 — https://xkx.me（Laravel CSRF + session cookie，中文临时邮箱）。
/// 创建：GET / 提取 csrf-token 与 session cookie → POST /mailbox/create/random（不跟随重定向），
///       从 302 Location 头正则提取邮箱地址；token 保存 cookie 头串。
/// 收信：GET /mailbox/{email}/messages（携带 cookie，Accept: application/json），404 视为空。
/// </summary>
public static class XkxMe
{
    private const string Channel = "xkx-me";
    private const string BaseUrl = "https://xkx.me";
    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";
    private static readonly Regex CsrfRe = new("csrf-token\" content=\"([^\"]+)\"", RegexOptions.Compiled);
    private static readonly Regex EmailRe = new("mailbox/([^/\\s\"'<>]+@xkx\\.me)", RegexOptions.Compiled);

    private static (string Csrf, string Cookie) GetSession()
    {
        var resp = Http.Get(BaseUrl, new Dictionary<string, string> { ["User-Agent"] = Ua });
        resp.EnsureSuccess();
        var m = CsrfRe.Match(resp.Body);
        if (!m.Success) throw new Exception($"{Channel}: 无法获取 CSRF token");
        return (m.Groups[1].Value, ProviderHttpUtil.CookieHeaderFrom(resp));
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var (csrf, cookie) = GetSession();
        var headers = new Dictionary<string, string>
        {
            ["User-Agent"] = Ua,
            ["Content-Type"] = "application/x-www-form-urlencoded",
        };
        if (!string.IsNullOrEmpty(cookie)) headers["Cookie"] = cookie;
        var body = $"_token={System.Net.WebUtility.UrlEncode(csrf)}";
        var resp = Http.PostNoRedirect($"{BaseUrl}/mailbox/create/random", body,
            "application/x-www-form-urlencoded", headers);

        resp.Headers.TryGetValue("Location", out var location);
        var m = EmailRe.Match(location ?? "");
        if (!m.Success) throw new Exception($"{Channel}: 无法从响应中提取邮箱地址");
        var email = m.Groups[1].Value;

        return new EmailInfo(Channel, email, cookie);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var address = (email ?? "").Trim();
        if (string.IsNullOrEmpty(address)) throw new Exception($"{Channel}: 缺少邮箱地址");

        var headers = new Dictionary<string, string>
        {
            ["User-Agent"] = Ua,
            ["Accept"] = "application/json",
            ["X-Requested-With"] = "XMLHttpRequest",
        };
        if (!string.IsNullOrEmpty(token)) headers["Cookie"] = token;

        var url = $"{BaseUrl}/mailbox/{System.Uri.EscapeDataString(address).Replace("%40", "@")}/messages";
        var resp = Http.Get(url, headers);
        if (resp.StatusCode == 404) return new List<Email>();
        resp.EnsureSuccess();

        var node = Json.Parse(resp.Body);
        JsonArray? messages = node switch
        {
            JsonArray arr => arr,
            JsonObject obj when obj["messages"] is JsonArray ma => ma,
            JsonObject obj when obj["message"] is JsonObject mo => new JsonArray(mo.DeepClone()),
            _ => null,
        };
        var result = new List<Email>();
        if (messages is null) return result;

        foreach (var it in messages)
        {
            if (it is not JsonObject msg) continue;
            var html = Json.Str(msg, "html");
            if (string.IsNullOrEmpty(html)) html = Json.Str(msg, "body");
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(msg, "id"),
                ["from"] = Json.Str(msg, "from"),
                ["to"] = address,
                ["subject"] = Json.Str(msg, "subject"),
                ["date"] = Json.Str(msg, "date"),
                ["html"] = html,
                ["text"] = Json.Str(msg, "text"),
            }, address));
        }
        return result;
    }
}
