using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// TempInbox 渠道（endpoint.tempinbox.xyz）。
/// 指定域名时 GET /email/{addr}；否则 GET /email/Random（返回带引号纯字符串地址）。
/// 收信：GET /messages/{email}。
/// </summary>
public static class Tempinbox
{
    private const string Base = "https://endpoint.tempinbox.xyz";
    private static readonly string[] Domains = { "tempinbox.xyz", "thepiratebay.cloud", "cryptoblad.nl" };
    private static readonly Random Rand = new();

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Cache-Control"] = "no-cache",
        ["Pragma"] = "no-cache",
        ["Referer"] = "https://www.tempinbox.xyz/",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static string RandomUser(int length = 10)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>创建临时邮箱（可选指定域名）</summary>
    public static EmailInfo Generate(string? domain)
    {
        string addr;
        var d = (domain ?? "").Trim().ToLowerInvariant();
        if (d.Length > 0)
        {
            if (Array.IndexOf(Domains, d) < 0) throw new Exception($"tempinbox: domain not available: {d}");
            var user = RandomUser();
            addr = $"{user}@{d}";
            var resp = Http.Get($"{Base}/email/{addr}", Headers());
            resp.EnsureSuccess();
        }
        else
        {
            var resp = Http.Get($"{Base}/email/Random", Headers());
            resp.EnsureSuccess();
            addr = (resp.Body ?? "").Trim().Trim('"');
        }
        if (addr.Length == 0 || !addr.Contains('@')) throw new Exception("tempinbox: invalid email address returned");
        return new EmailInfo("tempinbox", addr);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var e = (email ?? "").Trim();
        if (e.Length == 0) throw new Exception("tempinbox: empty email");
        var resp = Http.Get($"{Base}/messages/{e}", Headers());
        resp.EnsureSuccess();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var raw in rows)
            if (raw is JsonObject) result.Add(Normalize.NormalizeEmail(Json.ToDict(raw), e));
        return result;
    }
}
