using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// freecustom 渠道（freecustom.email）。免注册，任意 local part @ 可用域名即时可收信，
/// Token 即邮箱地址本身；读信时动态获取匿名 JWT。
/// </summary>
public static class FreeCustom
{
    private const string SiteUrl = "https://www.freecustom.email";
    private const string DomainsUrl = "https://api2.freecustom.email/domains";
    private const string Referer = "https://www.freecustom.email/en";
    private static readonly Random Rand = new();

    private static string Ua()
    {
        var cfg = Config.Get();
        if (cfg.Headers is not null && cfg.Headers.TryGetValue("User-Agent", out var ua) && !string.IsNullOrEmpty(ua))
            return ua;
        return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";
    }

    private static string RandomLocal(int n)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder();
        for (var i = 0; i < n; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string First(JsonObject msg, params string[] keys)
    {
        foreach (var key in keys)
        {
            if (msg[key] is JsonNode n)
            {
                var s = Json.NodeToString(n);
                if (!string.IsNullOrEmpty(s)) return s;
            }
        }
        return "";
    }

    private static string PickDomain()
    {
        var resp = Http.Get(DomainsUrl, new Dictionary<string, string>
        {
            ["User-Agent"] = Ua(),
            ["Accept"] = "application/json",
            ["Referer"] = Referer,
        });
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var lst = data?["data"] as JsonArray;
        if (lst is null || lst.Count == 0) throw new Exception("freecustom: 域名列表为空");

        var usable = new List<string>();
        var all = new List<string>();
        foreach (var d in lst)
        {
            if (d is not JsonObject o) continue;
            var domain = o["domain"]?.GetValue<string>();
            if (string.IsNullOrEmpty(domain)) continue;
            all.Add(domain);
            var tier = o["tier"]?.GetValue<string>();
            var expiringSoon = (o["expiring_soon"] as JsonValue)?.GetValue<bool>() ?? false;
            if (tier == "free" && !expiringSoon) usable.Add(domain);
        }
        var pool = usable.Count > 0 ? usable : all;
        if (pool.Count == 0) throw new Exception("freecustom: 无可用域名");
        return pool[Rand.Next(pool.Count)];
    }

    private static string FetchAuthToken()
    {
        var resp = Http.Post($"{SiteUrl}/api/auth", "", "application/json", new Dictionary<string, string>
        {
            ["User-Agent"] = Ua(),
            ["Accept"] = "application/json",
            ["Referer"] = Referer,
        });
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var token = data?["token"]?.GetValue<string>();
        if (string.IsNullOrEmpty(token)) throw new Exception("freecustom: 令牌响应无效");
        return token;
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var domain = PickDomain();
        var email = $"{RandomLocal(10)}@{domain}";
        return new EmailInfo("freecustom", email, email);
    }

    /// <summary>获取邮件列表，逐封补全正文</summary>
    public static List<Email> GetEmails(string email)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("freecustom: 缺少邮箱地址");

        var jwt = FetchAuthToken();
        var authHeaders = new Dictionary<string, string>
        {
            ["User-Agent"] = Ua(),
            ["Accept"] = "application/json",
            ["Referer"] = Referer,
            ["Authorization"] = $"Bearer {jwt}",
            ["x-fce-client"] = "web-client",
        };

        var listResp = Http.Get($"{SiteUrl}/api/public-mailbox?fullMailboxId={WebUtility.UrlEncode(addr)}", authHeaders);
        listResp.EnsureSuccess();
        var listData = Json.Parse(listResp.Body) as JsonObject;
        var result = new List<Email>();
        if (listData is null || (listData["success"] as JsonValue)?.GetValue<bool>() != true) return result;
        if (listData["data"] is not JsonArray items) return result;

        foreach (var item in items)
        {
            if (item is not JsonObject meta) continue;
            var msgId = First(meta, "id");
            if (string.IsNullOrEmpty(msgId)) continue;

            JsonObject full = meta;
            try
            {
                var msgResp = Http.Get(
                    $"{SiteUrl}/api/public-mailbox?fullMailboxId={WebUtility.UrlEncode(addr)}&messageId={WebUtility.UrlEncode(msgId)}",
                    authHeaders);
                if (msgResp.Ok)
                {
                    var msgData = Json.Parse(msgResp.Body) as JsonObject;
                    if (msgData is not null && (msgData["success"] as JsonValue)?.GetValue<bool>() == true
                        && msgData["data"] is JsonObject fullData)
                        full = fullData;
                }
            }
            catch { /* 正文补全失败退回列表元数据 */ }

            var row = new Dictionary<string, object?>
            {
                ["id"] = First(full, "id"),
                ["from"] = First(full, "from"),
                ["to"] = FirstOr(First(full, "to"), addr),
                ["subject"] = First(full, "subject"),
                ["text"] = First(full, "text"),
                ["html"] = First(full, "html"),
                ["date"] = First(full, "date"),
            };
            result.Add(Normalize.NormalizeEmail(row, addr));
        }
        return result;
    }

    private static string FirstOr(string value, string fallback) => value.Length > 0 ? value : fallback;
}
