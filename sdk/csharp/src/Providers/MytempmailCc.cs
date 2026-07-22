using System;
using System.Collections.Generic;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// MyTempMail.cc 渠道 — https://mytempmail.cc（API https://api.mytempmail.cc）
/// 简单 REST JSON API。域名 nilvaro.com，有效期 600 秒。
/// 创建：POST /api/address body JSON{"domain","name","expiry"} → {"token","address","expires_in"}。
/// 收信：GET /api/mails/{token} → {"results":[...]}。
/// token 存服务端返回 token。
/// </summary>
public static class MytempmailCc
{
    private const string BaseUrl = "https://api.mytempmail.cc";
    private const string DefaultDomain = "nilvaro.com";
    private const int DefaultExpiry = 600;

    private static readonly Random Rand = new();

    private static Dictionary<string, string> Headers() => new()
    {
        ["Content-Type"] = "application/json",
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    };

    private static string RandomName(int length = 10)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>创建 mytempmail.cc 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var name = RandomName();
        var payload = Json.Serialize(new Dictionary<string, object?>
        {
            ["domain"] = DefaultDomain,
            ["name"] = name,
            ["expiry"] = DefaultExpiry,
        });
        var resp = Http.Post($"{BaseUrl}/api/address", payload, "application/json", Headers());
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject
            ?? throw new Exception("mytempmail-cc: 响应格式无效");
        var token = Json.Str(data, "token");
        var address = Json.Str(data, "address");
        var expiresIn = data["expires_in"]?.GetValue<long?>() ?? DefaultExpiry;

        if (token.Length == 0 || address.Length == 0 || !address.Contains('@'))
            throw new Exception($"mytempmail-cc: 创建邮箱失败，响应: {resp.Body}");

        var expiresAt = (DateTimeOffset.UtcNow.ToUnixTimeSeconds() + expiresIn) * 1000;
        return new EmailInfo("mytempmail-cc", address, token, expiresAt);
    }

    /// <summary>获取 mytempmail.cc 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("mytempmail-cc: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("mytempmail-cc: 邮箱地址不能为空");

        var resp = Http.Get($"{BaseUrl}/api/mails/{token}", Headers());
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data?["results"] is not JsonArray results) return result;

        foreach (var node in results)
        {
            if (node is not JsonObject item) continue;
            var to = Json.Str(item, "to");
            if (to.Length == 0) to = addr;

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(item, "id"),
                ["from"] = FirstNonEmpty(Json.Str(item, "from"), Json.Str(item, "from_address"), Json.Str(item, "sender")),
                ["to"] = to,
                ["subject"] = Json.Str(item, "subject"),
                ["text"] = FirstNonEmpty(Json.Str(item, "text"), Json.Str(item, "body_text")),
                ["html"] = FirstNonEmpty(Json.Str(item, "html"), Json.Str(item, "body_html")),
                ["date"] = FirstNonEmpty(Json.Str(item, "date"), Json.Str(item, "received_at")),
            }, addr));
        }
        return result;
    }

    private static string FirstNonEmpty(params string[] values)
    {
        foreach (var v in values) if (!string.IsNullOrEmpty(v)) return v;
        return "";
    }
}
