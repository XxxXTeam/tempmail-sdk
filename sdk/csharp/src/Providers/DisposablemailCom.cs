using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// disposablemail.com 渠道（METRONET s.r.o. 后端，与 fakemail/minuteinbox 同结构）。
/// 创建：GET / 取 session cookie + CSRF（正则 CSRF="xxx"）→ GET /index/index?csrf_token= 建邮箱。
/// 收信：GET /index/refresh 取列表 → GET /email/id/{id} 取 HTML 正文。
/// token 编码 "dmc1:" + base64(JSON{t=csrf,c=cookie})。
/// </summary>
public static class DisposablemailCom
{
    private const string BaseUrl = "https://www.disposablemail.com";
    private const string TokPrefix = "dmc1:";

    // 从 HTML 中提取 CSRF token: CSRF="xxx"
    private static readonly Regex CsrfRe = new("CSRF=\"([^\"]+)\"", RegexOptions.Compiled);

    private const string Ua =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static Dictionary<string, string> BrowserHeaders() => new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    };

    private static Dictionary<string, string> AjaxHeaders(string cookieHdr)
    {
        var h = new Dictionary<string, string>
        {
            ["User-Agent"] = Ua,
            ["Accept"] = "application/json, text/javascript, */*; q=0.01",
            ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
            ["X-Requested-With"] = "XMLHttpRequest",
            ["Referer"] = $"{BaseUrl}/",
        };
        if (!string.IsNullOrEmpty(cookieHdr)) h["Cookie"] = cookieHdr;
        return h;
    }

    private static string EncodeToken(string csrf, string cookieHdr)
    {
        var raw = Json.Serialize(new Dictionary<string, object?> { ["t"] = csrf, ["c"] = cookieHdr });
        return TokPrefix + Convert.ToBase64String(Encoding.UTF8.GetBytes(raw));
    }

    private static (string csrf, string cookie) DecodeToken(string token)
    {
        if (!token.StartsWith(TokPrefix, StringComparison.Ordinal))
            throw new Exception("disposablemail-com: 无效的 token");
        string csrf, cookie;
        try
        {
            var raw = Encoding.UTF8.GetString(Convert.FromBase64String(token[TokPrefix.Length..]));
            var o = Json.Parse(raw) as JsonObject ?? throw new Exception();
            csrf = Json.Str(o, "t").Trim();
            cookie = Json.Str(o, "c").Trim();
        }
        catch { throw new Exception("disposablemail-com: 无效的 token"); }
        if (csrf.Length == 0) throw new Exception("disposablemail-com: token 中缺少 CSRF");
        return (csrf, cookie);
    }

    /// <summary>创建 disposablemail.com 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{BaseUrl}/", BrowserHeaders());
        resp.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);

        var m = CsrfRe.Match(resp.Body);
        if (!m.Success || m.Groups[1].Value.Length == 0)
            throw new Exception("disposablemail-com: 未能从首页提取 CSRF token");
        var csrf = m.Groups[1].Value;

        var resp2 = Http.Get($"{BaseUrl}/index/index?csrf_token={WebUtility.UrlEncode(csrf)}", AjaxHeaders(cookie));
        resp2.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, resp2);

        var data = Json.Parse(resp2.Body) as JsonObject
            ?? throw new Exception("disposablemail-com: 创建邮箱响应格式异常");
        var email = Json.Str(data, "email").Trim();
        if (email.Length == 0 || !email.Contains('@'))
            throw new Exception($"disposablemail-com: 获取到的邮箱地址无效: {email}");

        return new EmailInfo("disposablemail-com", email, EncodeToken(csrf, cookie));
    }

    /// <summary>获取 disposablemail.com 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("disposablemail-com: 邮箱地址为空");
        var (_, cookie) = DecodeToken((token ?? "").Trim());

        var resp = Http.Get($"{BaseUrl}/index/refresh", AjaxHeaders(cookie));
        resp.EnsureSuccess();

        var trimmed = (resp.Body ?? "").Trim();
        var result = new List<Email>();
        if (trimmed is "0" or "" or "[]") return result;

        if (Json.Parse(resp.Body) is not JsonArray list || list.Count == 0) return result;

        foreach (var node in list)
        {
            if (node is not JsonObject item) continue;
            var id = Json.Str(item, "id");
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = id,
                ["from"] = Json.Str(item, "od"),
                ["to"] = addr,
                ["subject"] = Json.Str(item, "predmet"),
                ["html"] = FetchBody(cookie, id),
                ["date"] = Json.Str(item, "kdy"),
                ["isRead"] = Json.Str(item, "precteno") == "precteno",
            }, addr));
        }
        return result;
    }

    private static string FetchBody(string cookieHdr, string mailId)
    {
        if (string.IsNullOrEmpty(mailId)) return "";
        try
        {
            var resp = Http.Get($"{BaseUrl}/email/id/{mailId}", AjaxHeaders(cookieHdr));
            return resp.Ok ? resp.Body : "";
        }
        catch { return ""; }
    }
}
