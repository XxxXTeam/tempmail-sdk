using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// MailholeDe 渠道（mailhole.de）。无需认证，token 即邮箱地址本身。
/// 创建：GET /api/random 从 HTML 中正则提取地址；收信：GET /json/{email}。
/// </summary>
public static class MailholeDe
{
    private const string Base = "https://mailhole.de";
    private static readonly Regex EmailRe = new(@"([a-z0-9.]+@mailhole\.de)", RegexOptions.Compiled);

    private static string First(JsonObject msg, params string[] keys)
    {
        foreach (var k in keys)
        {
            var v = Json.Str(msg, k);
            if (!string.IsNullOrEmpty(v)) return v;
        }
        return "";
    }

    /// <summary>创建 mailhole.de 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{Base}/api/random", new Dictionary<string, string> { ["Accept"] = "text/html" });
        resp.EnsureSuccess();
        var m = EmailRe.Match(resp.Body);
        if (!m.Success) throw new Exception("mailhole-de: 无法从响应中解析邮箱地址");
        var email = m.Groups[1].Value;
        return new EmailInfo("mailhole-de", email, email);
    }

    /// <summary>获取 mailhole.de 邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("mailhole-de: 缺少邮箱地址");
        var resp = Http.Get($"{Base}/json/{WebUtility.UrlEncode(addr)}",
            new Dictionary<string, string> { ["Accept"] = "application/json" });
        resp.EnsureSuccess();
        if (string.IsNullOrEmpty(resp.Body) || resp.Body == "[]") return new List<Email>();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var item in rows)
        {
            if (item is not JsonObject msg) continue;
            var raw = new Dictionary<string, object?>
            {
                ["id"] = Json.Str(msg, "id"),
                ["from"] = First(msg, "sender", "from"),
                ["to"] = addr,
                ["subject"] = Json.Str(msg, "subject"),
                ["text"] = First(msg, "body", "text"),
                ["html"] = First(msg, "html", "body"),
                ["received_at"] = First(msg, "date", "received"),
            };
            result.Add(Normalize.NormalizeEmail(raw, addr));
        }
        return result;
    }
}
