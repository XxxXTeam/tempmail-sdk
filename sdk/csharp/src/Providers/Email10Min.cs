using System;
using System.Collections.Generic;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// email10min 渠道 — https://email10min.com
/// 创建：GET /zh 取 CSRF + Cookie 并从 HTML 提取邮箱地址。
/// 收信：POST /messages?{ts} body(_token={csrf}&captcha=)。
/// token 编码 "e10m:" + base64(JSON{c=cookie,t=csrf})。
/// </summary>
public static class Email10Min
{
    private const string BaseUrl = "https://email10min.com";
    private const string TokPrefix = "e10m:";

    private static readonly Regex CsrfMetaRe = new("<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex CsrfInputRe = new("<input[^>]+name=\"_token\"[^>]+value=\"([^\"]+)\"", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex EmailIdRe = new("id=\"emailAddress\"[^>]*>([^<]+)", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex EmailClsRe = new("class=\"[^\"]*email[^\"]*\"[^>]*>([^<]*@[^<]+)", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex EmailDataRe = new("data-email=\"([^\"]+@[^\"]+)\"", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex EmailValueRe = new("value=\"([a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,})\"", RegexOptions.Compiled);
    private static readonly Regex EmailJsonRe = new("\"mailbox\"\\s*:\\s*\"([^\"]+@[^\"]+)\"", RegexOptions.Compiled);
    private static readonly Regex EmailGenericRe = new("([a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,})", RegexOptions.Compiled);

    private const string Ua =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static Dictionary<string, string> BrowserHeaders() => new()
    {
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["Cache-Control"] = "no-cache",
        ["DNT"] = "1",
        ["Pragma"] = "no-cache",
        ["Upgrade-Insecure-Requests"] = "1",
        ["User-Agent"] = Ua,
        ["sec-ch-ua"] = "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        ["sec-ch-ua-mobile"] = "?0",
        ["sec-ch-ua-platform"] = "\"Windows\"",
    };

    private static Dictionary<string, string> AjaxHeaders() => new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["Origin"] = BaseUrl,
        ["Referer"] = $"{BaseUrl}/zh",
        ["User-Agent"] = Ua,
        ["X-Requested-With"] = "XMLHttpRequest",
        ["sec-ch-ua"] = "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        ["sec-ch-ua-mobile"] = "?0",
        ["sec-ch-ua-platform"] = "\"Windows\"",
        ["sec-fetch-dest"] = "empty",
        ["sec-fetch-mode"] = "cors",
        ["sec-fetch-site"] = "same-origin",
    };

    private static string EncodeToken(string cookie, string csrf)
    {
        var raw = Json.Serialize(new Dictionary<string, object?> { ["c"] = cookie, ["t"] = csrf });
        return TokPrefix + Convert.ToBase64String(Encoding.UTF8.GetBytes(raw));
    }

    private static (string cookie, string csrf) DecodeToken(string token)
    {
        if (!token.StartsWith(TokPrefix, StringComparison.Ordinal))
            throw new Exception("email10min: invalid session token");
        string cookie, csrf;
        try
        {
            var raw = Encoding.UTF8.GetString(Convert.FromBase64String(token[TokPrefix.Length..]));
            var o = Json.Parse(raw) as JsonObject ?? throw new Exception();
            cookie = Json.Str(o, "c").Trim();
            csrf = Json.Str(o, "t").Trim();
        }
        catch { throw new Exception("email10min: invalid session token"); }
        if (cookie.Length == 0 || csrf.Length == 0)
            throw new Exception("email10min: invalid session token (empty fields)");
        return (cookie, csrf);
    }

    private static string ExtractCsrf(string html)
    {
        var m = CsrfMetaRe.Match(html);
        if (m.Success) return m.Groups[1].Value;
        m = CsrfInputRe.Match(html);
        if (m.Success) return m.Groups[1].Value;
        throw new Exception("email10min: 未找到 CSRF token");
    }

    private static string ExtractEmail(string html)
    {
        var m = EmailIdRe.Match(html);
        if (m.Success && m.Groups[1].Value.Contains('@')) return m.Groups[1].Value.Trim();
        m = EmailClsRe.Match(html);
        if (m.Success) return m.Groups[1].Value.Trim();
        m = EmailDataRe.Match(html);
        if (m.Success) return m.Groups[1].Value.Trim();
        m = EmailValueRe.Match(html);
        if (m.Success) return m.Groups[1].Value.Trim();
        m = EmailJsonRe.Match(html);
        if (m.Success) return m.Groups[1].Value.Trim();
        m = EmailGenericRe.Match(html);
        if (m.Success)
        {
            var addr = m.Groups[1].Value;
            if (!addr.Contains("email10min") && !addr.Contains("example") && !addr.Contains("google"))
                return addr.Trim();
        }
        throw new Exception("email10min: 未找到邮箱地址");
    }

    /// <summary>创建 email10min 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var r = Http.Get($"{BaseUrl}/zh", BrowserHeaders());
        r.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(r);
        var html = r.Body;
        var csrf = ExtractCsrf(html);
        var email = ExtractEmail(html);
        return new EmailInfo("email10min", email, EncodeToken(cookie, csrf));
    }

    /// <summary>获取 email10min 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var (cookie, csrf) = DecodeToken(token ?? "");
        var ts = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds().ToString();

        var headers = AjaxHeaders();
        headers["Cookie"] = cookie;

        var r = Http.Post($"{BaseUrl}/messages?{ts}", $"_token={csrf}&captcha=",
            "application/x-www-form-urlencoded; charset=UTF-8", headers);
        r.EnsureSuccess();

        var data = Json.Parse(r.Body) as JsonObject;
        var result = new List<Email>();
        if (data?["messages"] is not JsonArray messages) return result;

        var i = 0;
        foreach (var node in messages)
        {
            if (node is not JsonObject raw) { i++; continue; }
            var msgId = FirstNonEmpty(Json.Str(raw, "id"), Json.Str(raw, "message_id"), i.ToString());
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = msgId,
                ["from"] = FirstNonEmpty(Json.Str(raw, "from"), Json.Str(raw, "sender")),
                ["to"] = FirstNonEmpty(Json.Str(raw, "to"), email),
                ["subject"] = Json.Str(raw, "subject"),
                ["text"] = FirstNonEmpty(Json.Str(raw, "text"), Json.Str(raw, "body")),
                ["html"] = FirstNonEmpty(Json.Str(raw, "html"), Json.Str(raw, "body_html")),
                ["date"] = FirstNonEmpty(Json.Str(raw, "date"), Json.Str(raw, "created_at")),
                ["isRead"] = false,
            }, email));
            i++;
        }
        return result;
    }

    private static string FirstNonEmpty(params string[] values)
    {
        foreach (var v in values) if (!string.IsNullOrEmpty(v)) return v;
        return "";
    }
}
