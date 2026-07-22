using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Mail Sunls 渠道（mail.sunls.de）。
/// 无需 token/session：从 /api/domain 取域名列表本地随机拼地址；GET /api/fetch?to= 收信。
/// </summary>
public static class MailSunls
{
    private const string Base = "https://mail.sunls.de";
    private static readonly Random Rand = new();

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Cache-Control"] = "no-cache",
        ["Pragma"] = "no-cache",
        ["Referer"] = "https://mail.sunls.de/",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static string RandomLocal(int length = 10)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>创建邮箱：取域名列表后本地随机生成地址</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{Base}/api/domain", Headers());
        resp.EnsureSuccess();
        var arr = Json.Parse(resp.Body) as JsonArray
            ?? throw new Exception("mail-sunls: 域名列表响应格式无效");
        var domains = new List<string>();
        foreach (var d in arr)
        {
            var s = (d as JsonValue)?.GetValue<string>()?.Trim();
            if (!string.IsNullOrEmpty(s)) domains.Add(s);
        }
        if (domains.Count == 0) throw new Exception("mail-sunls: 无可用域名");
        var domain = domains[Rand.Next(domains.Count)];
        return new EmailInfo("mail-sunls", $"{RandomLocal()}@{domain}");
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("mail-sunls: 邮箱地址不能为空");
        var resp = Http.Get($"{Base}/api/fetch?to={WebUtility.UrlEncode(addr)}", Headers());
        resp.EnsureSuccess();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var raw in rows)
            if (raw is JsonObject) result.Add(Normalize.NormalizeEmail(Json.ToDict(raw), addr));
        return result;
    }
}
