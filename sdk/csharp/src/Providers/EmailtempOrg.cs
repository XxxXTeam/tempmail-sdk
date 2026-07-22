using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// emailtemp.org 渠道（Laravel + CSRF，与 tempmailten.com 共享模板）。
/// 创建：GET /en 取 session cookie + meta CSRF → POST /messages（body _token&captcha=）取邮箱。
/// 收信：POST /messages 取列表 → GET /view/{id} 取 HTML 正文。
/// token 编码 "eto1:" + base64(JSON{t=csrf,c=cookie})。
/// </summary>
public static class EmailtempOrg
{
    private const string BaseUrl = "https://emailtemp.org";
    private const string TokPrefix = "eto1:";

    private static readonly Regex CsrfRe = new("<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"", RegexOptions.Compiled);

    private const string Ua =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static Dictionary<string, string> BrowserHeaders() => new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    };

    private static string EncodeToken(string csrf, string cookieHdr)
    {
        var raw = Json.Serialize(new Dictionary<string, object?> { ["t"] = csrf, ["c"] = cookieHdr });
        return TokPrefix + Convert.ToBase64String(Encoding.UTF8.GetBytes(raw));
    }

    private static (string csrf, string cookie) DecodeToken(string token)
    {
        if (!token.StartsWith(TokPrefix, StringComparison.Ordinal))
            throw new Exception("emailtemp-org: 无效的 token");
        string csrf, cookie;
        try
        {
            var raw = Encoding.UTF8.GetString(Convert.FromBase64String(token[TokPrefix.Length..]));
            var o = Json.Parse(raw) as JsonObject ?? throw new Exception();
            csrf = Json.Str(o, "t").Trim();
            cookie = Json.Str(o, "c").Trim();
        }
        catch { throw new Exception("emailtemp-org: 无效的 token"); }
        if (csrf.Length == 0) throw new Exception("emailtemp-org: token 中缺少 CSRF");
        return (csrf, cookie);
    }

    private static JsonObject PostMessages(string csrf, string cookieHdr)
    {
        var headers = new Dictionary<string, string>
        {
            ["User-Agent"] = Ua,
            ["Accept"] = "application/json, text/javascript, */*; q=0.01",
            ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
            ["X-Requested-With"] = "XMLHttpRequest",
            ["Referer"] = $"{BaseUrl}/en",
            ["X-CSRF-TOKEN"] = csrf,
        };
        if (!string.IsNullOrEmpty(cookieHdr)) headers["Cookie"] = cookieHdr;

        var body = $"_token={WebUtility.UrlEncode(csrf)}&captcha=";
        var resp = Http.Post($"{BaseUrl}/messages", body, "application/x-www-form-urlencoded", headers);
        resp.EnsureSuccess();
        return Json.Parse(resp.Body) as JsonObject ?? throw new Exception("emailtemp-org: 解析响应 JSON 失败");
    }

    /// <summary>创建 emailtemp.org 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{BaseUrl}/en", BrowserHeaders());
        resp.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);

        var m = CsrfRe.Match(resp.Body);
        if (!m.Success) throw new Exception("emailtemp-org: 未能从首页提取 CSRF token");
        var csrf = m.Groups[1].Value;

        var data = PostMessages(csrf, cookie);
        var mailbox = Json.Str(data, "mailbox").Trim();
        if (mailbox.Length == 0 || !mailbox.Contains('@'))
            throw new Exception($"emailtemp-org: 邮箱地址无效: {mailbox}");

        return new EmailInfo("emailtemp-org", mailbox, EncodeToken(csrf, cookie));
    }

    /// <summary>获取 emailtemp.org 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("emailtemp-org: 邮箱地址为空");
        var (csrf, cookie) = DecodeToken((token ?? "").Trim());

        var data = PostMessages(csrf, cookie);
        var result = new List<Email>();
        if (data["messages"] is not JsonArray msgs || msgs.Count == 0) return result;

        foreach (var node in msgs)
        {
            if (node is not JsonObject msg) continue;
            var mid = Json.Str(msg, "id").Trim();
            if (mid.Length == 0) continue;

            var fromAddr = Json.Str(msg, "from_email");
            var fromName = Json.Str(msg, "from");
            if (!string.IsNullOrEmpty(fromName) && fromName != fromAddr)
                fromAddr = $"{fromName} <{fromAddr}>";

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = mid,
                ["from"] = fromAddr,
                ["to"] = addr,
                ["subject"] = Json.Str(msg, "subject"),
                ["html"] = FetchView(cookie, mid),
                ["isRead"] = (msg["is_seen"]?.GetValue<bool>() ?? false),
            }, addr));
        }
        return result;
    }

    private static string FetchView(string cookieHdr, string mid)
    {
        if (string.IsNullOrEmpty(mid)) return "";
        var headers = BrowserHeaders();
        headers["Referer"] = $"{BaseUrl}/en";
        if (!string.IsNullOrEmpty(cookieHdr)) headers["Cookie"] = cookieHdr;
        try
        {
            var resp = Http.Get($"{BaseUrl}/view/{WebUtility.UrlEncode(mid)}", headers);
            return resp.Ok ? resp.Body : "";
        }
        catch { return ""; }
    }
}
