using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// TempGmailer 渠道 — https://tempgmailer.com（Laravel + CSRF）。
/// 创建：GET / 提取 CSRF token 与 session cookie → POST /get-gmail 创建邮箱。
///       token 序列化保存 {"csrfToken","cookies"}。
/// 收信：POST /get-inbox（body: {"email","adblock":0}）。
/// </summary>
public static class TempGmailer
{
    private const string BaseUrl = "https://tempgmailer.com";
    private static readonly Regex CsrfRe = new("<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"", RegexOptions.Compiled);

    private static Dictionary<string, string> ApiHeaders(string csrf, string cookies) => new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Referer"] = $"{BaseUrl}/",
        ["X-Requested-With"] = "XMLHttpRequest",
        ["X-TempGmailer-Auth"] = "frontend",
        ["X-CSRF-TOKEN"] = csrf,
        ["Cookie"] = cookies,
    };

    private static (string Csrf, string Cookies) InitSession()
    {
        var resp = Http.Get(BaseUrl, new Dictionary<string, string>
        {
            ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        });
        resp.EnsureSuccess();
        var m = CsrfRe.Match(resp.Body);
        if (!m.Success) throw new Exception("tempgmailer: 无法提取 CSRF token");
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);
        if (string.IsNullOrEmpty(cookie)) throw new Exception("tempgmailer: 未获取到 session cookie");
        return (m.Groups[1].Value, cookie);
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var (csrf, cookies) = InitSession();
        var h = ApiHeaders(csrf, cookies);
        h["Content-Type"] = "application/json";
        var payload = Json.Serialize(new Dictionary<string, object?> { ["refresh"] = true, ["adblock"] = 0 });
        var resp = Http.Post($"{BaseUrl}/get-gmail", payload, "application/json", h);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (!((data?["success"] as JsonValue)?.GetValue<bool>() ?? false))
            throw new Exception("tempgmailer: 创建邮箱失败");
        var email = Json.Str(data?["data"] as JsonObject, "email").Trim();
        if (string.IsNullOrEmpty(email)) throw new Exception("tempgmailer: 创建邮箱失败, 未返回邮箱地址");
        var token = Json.Serialize(new Dictionary<string, object?> { ["csrfToken"] = csrf, ["cookies"] = cookies });
        return new EmailInfo("tempgmailer", email, token);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var session = Json.Parse(token ?? "") as JsonObject;
        var csrf = Json.Str(session, "csrfToken");
        var cookies = Json.Str(session, "cookies");
        var h = ApiHeaders(csrf, cookies);
        h["Content-Type"] = "application/json";
        var payload = Json.Serialize(new Dictionary<string, object?> { ["email"] = email, ["adblock"] = 0 });
        var resp = Http.Post($"{BaseUrl}/get-inbox", payload, "application/json", h);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (!((data?["success"] as JsonValue)?.GetValue<bool>() ?? false)) return result;
        if ((data?["data"] as JsonObject)?["messages"] is not JsonArray messages) return result;

        foreach (var m in messages)
        {
            if (m is not JsonObject msg) continue;
            var from = "";
            if (msg["from"] is JsonObject fo) from = Json.Str(fo, "address");
            else from = Json.Str(msg, "from");
            var html = !string.IsNullOrEmpty(Json.Str(msg, "body")) ? Json.Str(msg, "body") : Json.Str(msg, "intro");
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(msg, "id"),
                ["from"] = from,
                ["to"] = email,
                ["subject"] = Json.Str(msg, "subject"),
                ["text"] = Json.Str(msg, "intro"),
                ["html"] = html,
                ["date"] = Json.Str(msg, "createdAt"),
            }, email));
        }
        return result;
    }
}
