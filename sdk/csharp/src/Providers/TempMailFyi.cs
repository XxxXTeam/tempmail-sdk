using System;
using System.Collections.Generic;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// temp-mail.fyi 渠道 — https://temp-mail.fyi（PHP session + CSRF）。
/// 创建：GET / 获取 PHPSESSID cookie 并正则提取 csrfToken → POST /api/generate_email.php（body "{}"）。
///       token（前缀 tmf1: + base64）保存 {"t":csrf,"c":cookie}。
/// 收信：POST /api/get_emails.php，body {"email_address":email}，返回 {"success","emails":[...]}。
/// </summary>
public static class TempMailFyi
{
    private const string Channel = "tempmail-fyi";
    private const string BaseUrl = "https://temp-mail.fyi";
    private const string Prefix = "tmf1:";
    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";
    private static readonly Regex CsrfRe = new("csrfToken\"\\s*value=\"([^\"]+)\"", RegexOptions.Compiled);

    private static Dictionary<string, string> BrowserHeaders() => new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    };

    private static Dictionary<string, string> ApiHeaders(string csrf, string cookie)
    {
        var h = new Dictionary<string, string>
        {
            ["User-Agent"] = Ua,
            ["Accept"] = "application/json, text/javascript, */*; q=0.01",
            ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
            ["Content-Type"] = "application/json",
            ["X-CSRF-Token"] = csrf,
            ["X-Requested-With"] = "XMLHttpRequest",
            ["Referer"] = $"{BaseUrl}/",
        };
        if (!string.IsNullOrEmpty(cookie)) h["Cookie"] = cookie;
        return h;
    }

    private static string EncodeToken(string csrf, string cookie)
    {
        var raw = Encoding.UTF8.GetBytes(Json.Serialize(new Dictionary<string, object?> { ["t"] = csrf, ["c"] = cookie }));
        return Prefix + Convert.ToBase64String(raw);
    }

    private static (string Csrf, string Cookie) DecodeToken(string token)
    {
        if (!token.StartsWith(Prefix, StringComparison.Ordinal)) throw new Exception($"{Channel}: 无效的 token");
        JsonObject? o;
        try { o = Json.Parse(Encoding.UTF8.GetString(Convert.FromBase64String(token[Prefix.Length..]))) as JsonObject; }
        catch { throw new Exception($"{Channel}: 无效的 token"); }
        var csrf = Json.Str(o, "t").Trim();
        var cookie = Json.Str(o, "c").Trim();
        if (string.IsNullOrEmpty(csrf)) throw new Exception($"{Channel}: token 中缺少 CSRF");
        return (csrf, cookie);
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{BaseUrl}/", BrowserHeaders());
        resp.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);
        var m = CsrfRe.Match(resp.Body);
        if (!m.Success || string.IsNullOrEmpty(m.Groups[1].Value))
            throw new Exception($"{Channel}: 未能从首页提取 CSRF token");
        var csrf = m.Groups[1].Value;

        var resp2 = Http.Post($"{BaseUrl}/api/generate_email.php", "{}", "application/json", ApiHeaders(csrf, cookie));
        resp2.EnsureSuccess();
        var data = Json.Parse(resp2.Body) as JsonObject;
        if (!((data?["success"] as JsonValue)?.GetValue<bool>() ?? false))
            throw new Exception($"{Channel}: 创建邮箱失败");
        var email = Json.Str(data, "email_address").Trim();
        if (string.IsNullOrEmpty(email) || !email.Contains('@'))
            throw new Exception($"{Channel}: 获取到的邮箱地址无效");

        long? expires = null;
        if ((data?["expires_at"] as JsonValue)?.TryGetValue<long>(out var ex) ?? false) expires = ex;
        return new EmailInfo(Channel, email, EncodeToken(csrf, cookie), expires);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        email = (email ?? "").Trim();
        if (string.IsNullOrEmpty(email)) throw new Exception($"{Channel}: 邮箱地址为空");
        var (csrf, cookie) = DecodeToken((token ?? "").Trim());

        var body = Json.Serialize(new Dictionary<string, object?> { ["email_address"] = email });
        var resp = Http.Post($"{BaseUrl}/api/get_emails.php", body, "application/json", ApiHeaders(csrf, cookie));
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (!((data?["success"] as JsonValue)?.GetValue<bool>() ?? false))
            throw new Exception($"{Channel}: 获取邮件列表失败");

        var result = new List<Email>();
        if (data?["emails"] is not JsonArray emails) return result;
        foreach (var it in emails)
        {
            if (it is not JsonObject obj) continue;
            result.Add(Normalize.NormalizeEmail(Json.ToDict(obj), email));
        }
        return result;
    }
}
