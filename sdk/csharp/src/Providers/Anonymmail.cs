using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Anonymmail 渠道（anonymmail.net）。
/// POST 表单 API，需先建立会话 Cookie（共享 HttpClient 内置 CookieContainer 自动维护）。
/// </summary>
public static class Anonymmail
{
    private const string Base = "https://anonymmail.net";
    private static readonly Random Rand = new();

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "*/*",
        ["Content-Type"] = "application/x-www-form-urlencoded",
        ["Referer"] = "https://anonymmail.net/",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static string RandomUsername(int length = 10)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static List<string> FetchDomains()
    {
        var resp = Http.Post($"{Base}/api/getDomains", "", "application/x-www-form-urlencoded", Headers);
        resp.EnsureSuccess();
        var arr = Json.Parse(resp.Body) as JsonArray;
        var domains = new List<string>();
        if (arr is null) return domains;
        foreach (var item in arr)
        {
            if (item is not JsonObject o) continue;
            var d = (o["domain"]?.GetValue<string>() ?? "").Trim();
            if (d.Length > 0) domains.Add(d);
        }
        return domains;
    }

    /// <summary>创建临时邮箱：先 GET 首页获取会话 Cookie，再 POST /api/create</summary>
    public static EmailInfo Generate()
    {
        var domains = FetchDomains();
        if (domains.Count == 0) throw new Exception("anonymmail: no domains available");
        var domain = domains[Rand.Next(domains.Count)];
        var email = $"{RandomUsername()}@{domain}";

        // GET 首页建立会话 Cookie（共享客户端的 CookieContainer 自动携带）
        try { Http.Get($"{Base}/", Headers); } catch { /* 忽略预热失败 */ }

        var resp = Http.Post($"{Base}/api/create", $"email={email}", "application/x-www-form-urlencoded", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null || (data["success"] as JsonValue)?.GetValue<bool>() != true)
            throw new Exception("anonymmail: create inbox failed");
        var addr = (data["email"]?.GetValue<string>() ?? "").Trim();
        if (addr.Length == 0) throw new Exception("anonymmail: missing email in response");
        return new EmailInfo("anonymmail", addr);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var e = (email ?? "").Trim();
        if (e.Length == 0) throw new Exception("anonymmail: empty email");

        var resp = Http.Post($"{Base}/api/get", $"email={e}", "application/x-www-form-urlencoded", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data is null) return result;
        // 响应格式: {"email@domain": {"created_at": "...", "emails": [...]}}
        if (data[e] is not JsonObject inbox) return result;
        if (inbox["emails"] is not JsonArray rows) return result;

        foreach (var raw in rows)
        {
            if (raw is not JsonObject ro) continue;
            var dict = Json.ToDict(ro);
            // token 映射为 id
            if (dict.TryGetValue("token", out var tok) && !dict.ContainsKey("id"))
            {
                dict["id"] = tok;
                dict.Remove("token");
            }
            // body 映射为 html（body 含 HTML 内容）
            if (dict.TryGetValue("body", out var body) && !dict.ContainsKey("html"))
            {
                dict["html"] = body;
                dict.Remove("body");
            }
            result.Add(Normalize.NormalizeEmail(dict, e));
        }
        return result;
    }
}
