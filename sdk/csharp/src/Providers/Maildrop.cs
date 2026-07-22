using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Maildrop 渠道 — https://maildrop.cx（REST JSON API，与 maildrop-cc GraphQL 独立）。
/// 创建：GET /api/suffixes.php 取域名（排除 transformer.edu.kg），随机本地部分拼邮箱。
/// 收信：GET /api/emails.php?addr=&page=1&limit=20 取 emails 数组。
/// token 存邮箱地址本身。
/// </summary>
public static class Maildrop
{
    private const string Base = "https://maildrop.cx";
    private const string ExcludedSuffix = "transformer.edu.kg";
    private static readonly Random Rand = new();

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["Cache-Control"] = "no-cache",
        ["DNT"] = "1",
        ["Pragma"] = "no-cache",
        ["Referer"] = "https://maildrop.cx/zh-cn/app",
        ["sec-ch-ua"] = "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        ["sec-ch-ua-mobile"] = "?0",
        ["sec-ch-ua-platform"] = "\"Windows\"",
        ["sec-fetch-dest"] = "empty",
        ["sec-fetch-mode"] = "cors",
        ["sec-fetch-site"] = "same-origin",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        ["x-requested-with"] = "XMLHttpRequest",
    };

    private static string RandomLocal(int length = 10)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static List<string> FetchSuffixes()
    {
        var resp = Http.Get($"{Base}/api/suffixes.php", Headers());
        resp.EnsureSuccess();
        if (Json.Parse(resp.Body) is not JsonArray data)
            throw new Exception("maildrop: invalid suffixes response");

        var ex = ExcludedSuffix.ToLowerInvariant();
        var result = new List<string>();
        foreach (var node in data)
        {
            if (node is not JsonValue v || !v.TryGetValue<string>(out var s)) continue;
            var t = s.Trim();
            if (t.Length > 0 && t.ToLowerInvariant() != ex) result.Add(t);
        }
        if (result.Count == 0) throw new Exception("maildrop: no domains available");
        return result;
    }

    private static string PickSuffix(List<string> suffixes, string? preferred)
    {
        if (!string.IsNullOrWhiteSpace(preferred))
        {
            var p = preferred.Trim().ToLowerInvariant();
            foreach (var d in suffixes)
                if (d.ToLowerInvariant() == p) return d;
            throw new Exception($"maildrop: domain not available: {p}");
        }
        return suffixes[Rand.Next(suffixes.Count)];
    }

    private static string CxDateToIso(string s)
    {
        s = (s ?? "").Trim();
        if (s.Length == 19 && s[10] == ' ')
            return s[..10] + "T" + s[11..] + "Z";
        return s;
    }

    /// <summary>创建 maildrop 临时邮箱（域名来自 suffixes，排除 transformer.edu.kg）</summary>
    public static EmailInfo Generate(string? domain)
    {
        var suffixes = FetchSuffixes();
        var dom = PickSuffix(suffixes, domain);
        var email = $"{RandomLocal()}@{dom}";
        return new EmailInfo("maildrop", email, email);
    }

    /// <summary>获取 maildrop 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) addr = (token ?? "").Trim();
        if (addr.Length == 0) throw new Exception("maildrop: empty address");

        var qs = $"addr={WebUtility.UrlEncode(addr)}&page=1&limit=20";
        var resp = Http.Get($"{Base}/api/emails.php?{qs}", Headers());
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data?["emails"] is not JsonArray rows) return result;

        foreach (var node in rows)
        {
            if (node is not JsonObject row) continue;
            var desc = Json.Str(row, "description").Trim();
            var ir = row["isRead"];
            var isRead = (ir is JsonValue b && b.TryGetValue<bool>(out var bv) && bv)
                || (ir is JsonValue n && n.TryGetValue<int>(out var nv) && nv == 1);

            result.Add(new Email
            {
                Id = Json.Str(row, "id"),
                From = Json.Str(row, "from_addr").Trim(),
                To = addr,
                Subject = Json.Str(row, "subject").Trim(),
                Text = desc,
                Html = "",
                Date = CxDateToIso(Json.Str(row, "date")),
                IsRead = isRead,
                Attachments = new List<EmailAttachment>(),
            });
        }
        return result;
    }
}
