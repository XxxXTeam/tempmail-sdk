using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// apihz（接口盒子）渠道 — https://apihz.cn
/// 需 id/key 凭据，内置官方公共账号 88888888/88888888，可用 APIHZ_ID/APIHZ_KEY 覆盖。
/// 创建：GET /api/mail/mailcache.php 建邮箱（有效期 10 分钟）。
/// 收信：GET /api/mail/mailgetlist.php（须带创建时的 pwd）。
/// token 编码 "apihz1:" + base64(JSON{mail,pwd})。
/// </summary>
public static class Apihz
{
    private const string BaseUrl = "https://cn.apihz.cn";
    private const string TokPrefix = "apihz1:";
    private const string PublicId = "88888888";
    private const string PublicKey = "88888888";

    // apihz 自研收信域名
    private static readonly string[] Domains = { "apimail.email", "apimail.vip" };
    private static readonly Random Rand = new();

    private static string Ua()
    {
        var cfg = Config.Get();
        if (cfg.Headers is not null && cfg.Headers.TryGetValue("User-Agent", out var ua) && !string.IsNullOrEmpty(ua))
            return ua;
        return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";
    }

    private static (string id, string key) Credentials()
    {
        var cfg = Config.Get();
        var id = string.IsNullOrWhiteSpace(cfg.ApihzId) ? PublicId : cfg.ApihzId!.Trim();
        var key = string.IsNullOrWhiteSpace(cfg.ApihzKey) ? PublicKey : cfg.ApihzKey!.Trim();
        return (id, key);
    }

    private static string RandomLocal(int n)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < n; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string RandomPassword()
    {
        const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < 12; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string EncToken(string mail, string pwd)
    {
        var raw = Json.Serialize(new Dictionary<string, object?> { ["mail"] = mail, ["pwd"] = pwd });
        return TokPrefix + Convert.ToBase64String(Encoding.UTF8.GetBytes(raw));
    }

    private static (string mail, string pwd) DecToken(string? tok)
    {
        if (string.IsNullOrEmpty(tok) || !tok.StartsWith(TokPrefix, StringComparison.Ordinal))
            throw new Exception("apihz: 无效 token");
        string mail, pwd;
        try
        {
            var raw = Encoding.UTF8.GetString(Convert.FromBase64String(tok[TokPrefix.Length..]));
            var obj = Json.Parse(raw) as JsonObject ?? throw new Exception();
            mail = Json.Str(obj, "mail").Trim();
            pwd = Json.Str(obj, "pwd").Trim();
        }
        catch { throw new Exception("apihz: 无效 token"); }
        if (mail.Length == 0 || pwd.Length == 0) throw new Exception("apihz: 无效 token");
        return (mail, pwd);
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

    /// <summary>创建 apihz 临时邮箱，有效期 10 分钟</summary>
    public static EmailInfo Generate()
    {
        var (id, key) = Credentials();
        var domain = Domains[Rand.Next(Domains.Length)];
        var name = RandomLocal(10);
        var pwd = RandomPassword();

        var url = $"{BaseUrl}/api/mail/mailcache.php?id={WebUtility.UrlEncode(id)}&key={WebUtility.UrlEncode(key)}" +
                  $"&domain={WebUtility.UrlEncode(domain)}&name={WebUtility.UrlEncode(name)}&pwd={WebUtility.UrlEncode(pwd)}&buytype=0";
        var resp = Http.Get(url, new Dictionary<string, string> { ["User-Agent"] = Ua(), ["Accept"] = "application/json" });
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        var code = data?["code"]?.GetValue<int>() ?? 0;
        var mail = Json.Str(data, "mail");
        if (data is null || code != 200 || string.IsNullOrEmpty(mail))
            throw new Exception($"apihz: 创建邮箱失败 {resp.Body}");

        // 优先使用响应回传的 pwd，确保读信密码正确
        var finalPwd = Json.Str(data, "pwd");
        if (string.IsNullOrEmpty(finalPwd)) finalPwd = pwd;

        return new EmailInfo("apihz", mail, EncToken(mail, finalPwd));
    }

    /// <summary>获取 apihz 邮件列表（须带创建时的 mail 与 pwd）</summary>
    public static List<Email> GetEmails(string? token)
    {
        var (mail, pwd) = DecToken(token);
        var (id, key) = Credentials();

        var url = $"{BaseUrl}/api/mail/mailgetlist.php?id={WebUtility.UrlEncode(id)}&key={WebUtility.UrlEncode(key)}" +
                  $"&mail={WebUtility.UrlEncode(mail)}&pwd={WebUtility.UrlEncode(pwd)}&page=1";
        var resp = Http.Get(url, new Dictionary<string, string> { ["User-Agent"] = Ua(), ["Accept"] = "application/json" });
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        var code = data?["code"]?.GetValue<int>() ?? 0;
        var result = new List<Email>();
        if (data is null || code != 200) return result;

        if (data["data"] is not JsonArray items) return result;
        foreach (var node in items)
        {
            if (node is not JsonObject msg) continue;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = First(msg, "time1"),
                ["from"] = First(msg, "frommail", "fromname"),
                ["to"] = mail,
                ["subject"] = First(msg, "subject"),
                ["text"] = First(msg, "content"),
                ["html"] = First(msg, "content"),
                ["date"] = First(msg, "time2", "time1"),
            }, mail));
        }
        return result;
    }
}
