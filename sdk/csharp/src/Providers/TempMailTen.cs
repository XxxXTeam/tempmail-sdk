using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// tempmailten.com / tempp-mails.com 共享的 Laravel + CSRF 临时邮箱模板实现。
/// 创建：GET /en 获取 session cookie + meta CSRF token → POST /messages（_token=csrf&captcha=）取邮箱。
///       token 序列化（前缀 + base64）保存 {"t":csrf,"c":cookie}。
/// 收信：POST /messages 取 messages，逐封 GET /view/{id} 取 HTML 正文。
/// </summary>
public static class TempMailTenTemplate
{
    private static readonly Regex CsrfRe = new("<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"", RegexOptions.Compiled);
    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static Dictionary<string, string> BrowserHeaders(string baseUrl) => new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    };

    private static Dictionary<string, string> AjaxHeaders(string baseUrl) => new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "application/json, text/javascript, */*; q=0.01",
        ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        ["X-Requested-With"] = "XMLHttpRequest",
        ["Referer"] = $"{baseUrl}/en",
    };

    private static string EncodeToken(string prefix, string csrf, string cookie)
    {
        var raw = Encoding.UTF8.GetBytes(Json.Serialize(new Dictionary<string, object?> { ["t"] = csrf, ["c"] = cookie }));
        return prefix + Convert.ToBase64String(raw);
    }

    private static (string Csrf, string Cookie) DecodeToken(string channel, string prefix, string token)
    {
        if (!token.StartsWith(prefix, StringComparison.Ordinal)) throw new Exception($"{channel}: 无效的 token");
        JsonObject? o;
        try { o = Json.Parse(Encoding.UTF8.GetString(Convert.FromBase64String(token[prefix.Length..]))) as JsonObject; }
        catch { throw new Exception($"{channel}: 无效的 token"); }
        var csrf = Json.Str(o, "t").Trim();
        var cookie = Json.Str(o, "c").Trim();
        if (string.IsNullOrEmpty(csrf)) throw new Exception($"{channel}: token 中缺少 CSRF");
        return (csrf, cookie);
    }

    private static JsonObject PostMessages(string channel, string baseUrl, string csrf, string cookie)
    {
        var headers = AjaxHeaders(baseUrl);
        headers["Content-Type"] = "application/x-www-form-urlencoded";
        headers["X-CSRF-TOKEN"] = csrf;
        if (!string.IsNullOrEmpty(cookie)) headers["Cookie"] = cookie;
        var body = $"_token={WebUtility.UrlEncode(csrf)}&captcha=";
        var resp = Http.Post($"{baseUrl}/messages", body, "application/x-www-form-urlencoded", headers);
        resp.EnsureSuccess();
        return Json.Parse(resp.Body) as JsonObject ?? throw new Exception($"{channel}: 解析响应 JSON 失败");
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate(string channel, string baseUrl, string prefix)
    {
        var resp = Http.Get($"{baseUrl}/en", BrowserHeaders(baseUrl));
        resp.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);
        var m = CsrfRe.Match(resp.Body);
        if (!m.Success) throw new Exception($"{channel}: 未能从首页提取 CSRF token");
        var csrf = m.Groups[1].Value;

        var data = PostMessages(channel, baseUrl, csrf, cookie);
        var mailbox = Json.Str(data, "mailbox").Trim();
        if (string.IsNullOrEmpty(mailbox) || !mailbox.Contains('@'))
            throw new Exception($"{channel}: 邮箱地址无效");
        return new EmailInfo(channel, mailbox, EncodeToken(prefix, csrf, cookie));
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string channel, string baseUrl, string prefix, string email, string? token)
    {
        email = (email ?? "").Trim();
        if (string.IsNullOrEmpty(email)) throw new Exception($"{channel}: 邮箱地址为空");
        var (csrf, cookie) = DecodeToken(channel, prefix, (token ?? "").Trim());

        var data = PostMessages(channel, baseUrl, csrf, cookie);
        var msgs = data["messages"] as JsonArray;
        var result = new List<Email>();
        if (msgs is null) return result;

        foreach (var mm in msgs)
        {
            if (mm is not JsonObject msg) continue;
            var mid = Json.Str(msg, "id").Trim();
            if (string.IsNullOrEmpty(mid)) continue;

            var fromAddr = Json.Str(msg, "from_email");
            var fromName = Json.Str(msg, "from");
            if (!string.IsNullOrEmpty(fromName) && fromName != fromAddr) fromAddr = $"{fromName} <{fromAddr}>";

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = mid,
                ["from"] = fromAddr,
                ["to"] = email,
                ["subject"] = Json.Str(msg, "subject"),
                ["html"] = FetchView(baseUrl, cookie, mid),
                ["isRead"] = (msg["is_seen"] as JsonValue)?.GetValue<bool>() ?? false,
            }, email));
        }
        return result;
    }

    private static string FetchView(string baseUrl, string cookie, string mid)
    {
        if (string.IsNullOrEmpty(mid)) return "";
        var headers = BrowserHeaders(baseUrl);
        headers["Referer"] = $"{baseUrl}/en";
        if (!string.IsNullOrEmpty(cookie)) headers["Cookie"] = cookie;
        HttpResult resp;
        try { resp = Http.Get($"{baseUrl}/view/{WebUtility.UrlEncode(mid)}", headers); }
        catch { return ""; }
        if (resp.StatusCode is < 200 or >= 300) return "";
        return resp.Body;
    }
}

/// <summary>tempmailten.com 渠道（Laravel + CSRF 模板）。</summary>
public static class TempMailTen
{
    private const string BaseUrl = "https://tempmailten.com";
    private const string Prefix = "tmt1:";

    public static EmailInfo Generate() => TempMailTenTemplate.Generate("tempmailten", BaseUrl, Prefix);

    public static List<Email> GetEmails(string email, string? token)
        => TempMailTenTemplate.GetEmails("tempmailten", BaseUrl, Prefix, email, token);
}

/// <summary>tempp-mails.com 渠道（与 tempmailten 共享 Laravel + CSRF 模板）。</summary>
public static class TemppMails
{
    private const string BaseUrl = "https://tempp-mails.com";
    private const string Prefix = "tpm1:";

    public static EmailInfo Generate() => TempMailTenTemplate.Generate("tempp-mails", BaseUrl, Prefix);

    public static List<Email> GetEmails(string email, string? token)
        => TempMailTenTemplate.GetEmails("tempp-mails", BaseUrl, Prefix, email, token);
}
