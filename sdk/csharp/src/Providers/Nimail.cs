using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// nimail 渠道（www.nimail.cn）。POST 表单 API，无需认证，token 即邮箱地址本身。
/// 创建：POST /api/applymail (mail=前缀@nimail.cn)；收信：POST /api/getmails (mail=&amp;time=0)。
/// </summary>
public static class Nimail
{
    private const string Base = "https://www.nimail.cn";
    private static readonly Random Rand = new();

    private static string Ua()
    {
        var cfg = Config.Get();
        if (cfg.Headers is not null && cfg.Headers.TryGetValue("User-Agent", out var ua) && !string.IsNullOrEmpty(ua))
            return ua;
        return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";
    }

    private static Dictionary<string, string> FormHeaders() => new()
    {
        ["User-Agent"] = Ua(),
        ["Content-Type"] = "application/x-www-form-urlencoded",
        ["Origin"] = Base,
        ["Referer"] = $"{Base}/",
    };

    private static string RandomLocal(int n)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < n; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string First(JsonObject msg, params string[] keys)
    {
        foreach (var k in keys)
        {
            var v = Json.Str(msg, k);
            if (!string.IsNullOrEmpty(v)) return v;
        }
        return "";
    }

    /// <summary>创建 nimail.cn 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var email = $"{RandomLocal(10)}@nimail.cn";
        var body = $"mail={WebUtility.UrlEncode(email)}";
        var resp = Http.Post($"{Base}/api/applymail", body, "application/x-www-form-urlencoded", FormHeaders());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null || Json.Str(data, "success") != "true" || string.IsNullOrEmpty(Json.Str(data, "user")))
            throw new Exception("nimail: 创建邮箱失败");
        var user = Json.Str(data, "user");
        return new EmailInfo("nimail", user, user);
    }

    /// <summary>获取 nimail.cn 邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("nimail: 缺少邮箱地址");
        var body = $"mail={WebUtility.UrlEncode(addr)}&time=0";
        var resp = Http.Post($"{Base}/api/getmails", body, "application/x-www-form-urlencoded", FormHeaders());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data is null || Json.Str(data, "success") != "true") return result;
        if (data["mail"] is not JsonArray mail) return result;
        foreach (var item in mail)
        {
            if (item is not JsonObject msg) continue;
            var raw = new Dictionary<string, object?>
            {
                ["id"] = First(msg, "id", "time"),
                ["from"] = First(msg, "from", "sender"),
                ["to"] = addr,
                ["subject"] = First(msg, "subject", "title"),
                ["text"] = First(msg, "text", "content"),
                ["html"] = First(msg, "html", "content"),
                ["date"] = First(msg, "date", "time"),
            };
            result.Add(Normalize.NormalizeEmail(raw, addr));
        }
        return result;
    }
}
