using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// 24mail.chacuo.net 渠道 — http://24mail.chacuo.net
/// 创建：POST / form(data={name}@{domain}&type=refresh&arg=)。
/// 收信：POST / form(data={email}&type=refresh&arg=)。
/// token 存邮箱地址本身。域名 chacuo.net / 027168.com。
/// </summary>
public static class ChacuoMail
{
    private const string BaseUrl = "http://24mail.chacuo.net/";
    private const string DefaultDomain = "chacuo.net";
    private static readonly string[] Domains = { "chacuo.net", "027168.com" };
    private static readonly Random Rand = new();

    private const string Ua =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static string RandomLocal()
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder("sdk");
        for (var i = 0; i < 12; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string PickDomain(string? preferred)
    {
        var p = (preferred ?? "").Trim().TrimStart('@').ToLowerInvariant();
        if (p.Length > 0)
            foreach (var d in Domains)
                if (d.ToLowerInvariant() == p) return d;
        return DefaultDomain;
    }

    private static Dictionary<string, string> Headers() => new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "application/json, text/javascript, */*; q=0.01",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["X-Requested-With"] = "XMLHttpRequest",
        ["Origin"] = "http://24mail.chacuo.net",
        ["Referer"] = "http://24mail.chacuo.net/",
    };

    private static JsonObject Refresh(string email)
    {
        var body = $"data={WebUtility.UrlEncode(email)}&type=refresh&arg=";
        var resp = Http.Post(BaseUrl, body, "application/x-www-form-urlencoded; charset=UTF-8", Headers());
        resp.EnsureSuccess();
        return Json.Parse(resp.Body) as JsonObject ?? throw new Exception("24mail-chacuo: 响应格式异常");
    }

    /// <summary>创建 24mail.chacuo.net 临时邮箱</summary>
    public static EmailInfo Generate(string? domain)
    {
        var email = $"{RandomLocal()}@{PickDomain(domain)}";
        var result = Refresh(email);
        var status = result["status"]?.GetValue<int>() ?? 0;
        if (status != 1)
            throw new Exception($"24mail-chacuo: 创建邮箱失败: {Json.Str(result, "info")}");
        return new EmailInfo("24mail-chacuo", email, email);
    }

    /// <summary>获取 24mail.chacuo.net 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) addr = (token ?? "").Trim();
        if (addr.Length == 0) throw new Exception("24mail-chacuo: 邮箱地址为空");

        var result = Refresh(addr);
        var list = new List<Email>();
        if (result["data"] is not JsonArray data || data.Count == 0) return list;
        if (data[0] is not JsonObject first) return list;
        if (first["list"] is not JsonArray rows) return list;

        foreach (var node in rows)
        {
            if (node is not JsonObject item) continue;
            list.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(item, "MID"),
                ["from"] = Json.Str(item, "FROM"),
                ["to"] = addr,
                ["subject"] = Json.Str(item, "SUBJECT"),
                ["content"] = Json.Str(item, "CONTENT"),
                ["date"] = Json.Str(item, "TIME"),
            }, addr));
        }
        return list;
    }
}
