using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// MailGolem 渠道 — https://mailgolem.com
/// 基于 Laravel session + CSRF token。
/// 创建：GET / 取 session cookie + CSRF（input name="_token"）→ GET /random-email-address 取邮箱。
/// 收信：GET / 重取新 session + CSRF → POST /fetch-emails/{email}（body _token=）取列表。
/// token 编码 "mgolem1:" + base64(JSON{csrf,c=cookie})。
/// </summary>
public static class Mailgolem
{
    private const string Base = "https://mailgolem.com";
    private const string TokPrefix = "mgolem1:";

    private static readonly Regex CsrfRe = new("<input[^>]+name=\"_token\"[^>]+value=\"([^\"]+)\"", RegexOptions.Compiled);

    private static Dictionary<string, string> DefaultHeaders() => new()
    {
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        ["Cache-Control"] = "no-cache",
        ["DNT"] = "1",
        ["Pragma"] = "no-cache",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static string ExtractCsrf(string html)
    {
        var m = CsrfRe.Match(html);
        if (!m.Success) throw new Exception("mailgolem: 无法从页面中提取 CSRF token");
        return m.Groups[1].Value;
    }

    private static string EncodeToken(string csrf, string cookie)
    {
        var raw = Json.Serialize(new Dictionary<string, object?> { ["csrf"] = csrf, ["c"] = cookie });
        return TokPrefix + Convert.ToBase64String(Encoding.UTF8.GetBytes(raw));
    }

    /// <summary>创建 mailgolem 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var r1 = Http.Get($"{Base}/", DefaultHeaders());
        r1.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(r1);
        var csrf = ExtractCsrf(r1.Body);

        var headers = DefaultHeaders();
        headers["Referer"] = $"{Base}/";
        if (!string.IsNullOrEmpty(cookie)) headers["Cookie"] = cookie;
        var r2 = Http.Get($"{Base}/random-email-address", headers);
        r2.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, r2);

        var emailAddr = r2.Body.Trim();
        if (emailAddr.Length == 0 || !emailAddr.Contains('@'))
            throw new Exception($"mailgolem: 获取到无效的邮箱地址: {emailAddr}");

        return new EmailInfo("mailgolem", emailAddr, EncodeToken(csrf, cookie));
    }

    /// <summary>获取 mailgolem 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("mailgolem: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("mailgolem: 邮箱地址不能为空");

        // 重新获取 session 和 CSRF（原 session 可能已过期）
        var r1 = Http.Get($"{Base}/", DefaultHeaders());
        r1.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(r1);
        var newCsrf = ExtractCsrf(r1.Body);

        var headers = new Dictionary<string, string>
        {
            ["Accept"] = "application/json, text/plain, */*",
            ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
            ["Content-Type"] = "application/x-www-form-urlencoded",
            ["X-Requested-With"] = "XMLHttpRequest",
            ["Referer"] = $"{Base}/",
            ["User-Agent"] = DefaultHeaders()["User-Agent"],
        };
        if (!string.IsNullOrEmpty(cookie)) headers["Cookie"] = cookie;

        var r2 = Http.Post($"{Base}/fetch-emails/{addr}", $"_token={newCsrf}",
            "application/x-www-form-urlencoded", headers);
        r2.EnsureSuccess();

        var result = new List<Email>();
        if (Json.Parse(r2.Body) is not JsonArray body) return result;

        foreach (var node in body)
        {
            if (node is not JsonObject item) continue;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(item, "id"),
                ["from"] = Json.Str(item, "from"),
                ["to"] = addr,
                ["subject"] = Json.Str(item, "subject"),
            }, addr));
        }
        return result;
    }
}
