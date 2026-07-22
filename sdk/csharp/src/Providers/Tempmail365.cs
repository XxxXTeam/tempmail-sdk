using System;
using System.Collections.Generic;
using System.Globalization;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Tempmail365 渠道（tempmail365.cn/tempemail.php）。
/// 创建：get_config 取域名（失败用后备）→ create_email；收信：fetch_mail 返回 HTML content，正则提取发件人/主题。
/// </summary>
public static class Tempmail365
{
    private const string Base = "https://tempmail365.cn/tempemail.php";
    private static readonly string[] FallbackDomains = { "fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn" };
    private static readonly Random Rand = new();
    private static readonly Regex SenderRe = new(@"(?:发件人|From)\s*[:：]\s*(.+?)(?:<br|</|<p|\n|\r)", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex SubjectRe = new(@"(?:主题|Subject)\s*[:：]\s*(.+?)(?:<br|</|<p|\n|\r)", RegexOptions.IgnoreCase | RegexOptions.Compiled);

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Cache-Control"] = "no-cache",
        ["Pragma"] = "no-cache",
        ["Referer"] = "https://tempmail365.cn/",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static List<string> FetchDomains()
    {
        try
        {
            var resp = Http.Get($"{Base}?action=get_config", Headers());
            resp.EnsureSuccess();
            if (Json.Parse(resp.Body) is not JsonObject data) return new List<string>(FallbackDomains);
            if (data["domains"] is not JsonArray raw || raw.Count == 0) return new List<string>(FallbackDomains);
            var outList = new List<string>();
            foreach (var d in raw)
            {
                var s = (d as JsonValue)?.GetValue<string>()?.Trim();
                if (!string.IsNullOrEmpty(s)) outList.Add(s);
            }
            return outList.Count > 0 ? outList : new List<string>(FallbackDomains);
        }
        catch { return new List<string>(FallbackDomains); }
    }

    private static string RandomUsername(int length = 8)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string Extract(Regex re, string content)
    {
        var m = re.Match(content);
        return m.Success ? m.Groups[1].Value.Trim() : "";
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate(string? domain)
    {
        var domains = FetchDomains();
        if (domains.Count == 0) throw new Exception("tempmail365: 无可用域名");

        string selected;
        var d = (domain ?? "").Trim().ToLowerInvariant();
        if (d.Length > 0)
        {
            selected = domains.Find(x => x.ToLowerInvariant() == d)
                ?? throw new Exception($"tempmail365: 域名不可用: {d}");
        }
        else
        {
            selected = domains[Rand.Next(domains.Count)];
        }

        var addr = $"{RandomUsername()}@{selected}";
        var url = $"{Base}?action=create_email&email={WebUtility.UrlEncode(addr)}&domain={WebUtility.UrlEncode(selected)}";
        var resp = Http.Get(url, Headers());
        resp.EnsureSuccess();
        if (Json.Parse(resp.Body) is not JsonObject data || (data["success"] as JsonValue)?.GetValue<bool>() != true)
            throw new Exception("tempmail365: 创建邮箱失败");
        return new EmailInfo("tempmail365", addr);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var e = (email ?? "").Trim();
        if (e.Length == 0) throw new Exception("tempmail365: 邮箱地址为空");
        var resp = Http.Get($"{Base}?action=fetch_mail&email={WebUtility.UrlEncode(e)}", Headers());
        resp.EnsureSuccess();
        if (Json.Parse(resp.Body) is not JsonObject data) return new List<Email>();
        var content = Json.Str(data, "content");
        if (string.IsNullOrEmpty(content)) return new List<Email>();
        if (content.Trim() == "无邮件") return new List<Email>();

        var raw = new Dictionary<string, object?>
        {
            ["from"] = Extract(SenderRe, content),
            ["subject"] = Extract(SubjectRe, content),
            ["body"] = content,
            ["date"] = DateTimeOffset.UtcNow.ToString("o", CultureInfo.InvariantCulture),
        };
        return new List<Email> { Normalize.NormalizeEmail(raw, e) };
    }
}
