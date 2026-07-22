using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Fake Legal 渠道家族 — https://imgui.de
/// 动态获取域名列表；imgui.de / pulsewebmenu.de 通过 POST 保根域创建，其余随机域用 GET 创建。
/// </summary>
public static class FakeLegal
{
    private const string Base = "https://imgui.de";
    private static readonly Random Rand = new();

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Cache-Control"] = "no-cache",
        ["DNT"] = "1",
        ["Pragma"] = "no-cache",
        ["Referer"] = "https://imgui.de/",
        ["sec-ch-ua"] = "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        ["sec-ch-ua-mobile"] = "?0",
        ["sec-ch-ua-platform"] = "\"Windows\"",
        ["sec-fetch-dest"] = "empty",
        ["sec-fetch-mode"] = "cors",
        ["sec-fetch-site"] = "same-origin",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static List<string> FetchDomains()
    {
        var resp = Http.Get($"{Base}/api/domains", Headers);
        resp.EnsureSuccess();
        var raw = (Json.Parse(resp.Body) as JsonObject)?["domains"] as JsonArray;
        var result = new List<string>();
        if (raw is null) return result;
        foreach (var d in raw)
            if (d is JsonValue v && v.TryGetValue<string>(out var s) && !string.IsNullOrWhiteSpace(s))
                result.Add(s.Trim());
        return result;
    }

    private static string PickDomain(List<string> domains, string? preferred)
    {
        if (!string.IsNullOrWhiteSpace(preferred))
        {
            var p = preferred.Trim().ToLowerInvariant();
            foreach (var d in domains)
                if (d.ToLowerInvariant() == p) return d;
            throw new Exception($"fake-legal: domain not available: {p}");
        }
        return domains[Rand.Next(domains.Count)];
    }

    private static string RandomUsername(int length)
    {
        const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>创建 fake-legal 临时邮箱（指定渠道标识与域名）</summary>
    public static EmailInfo Generate(string channel, string? domain)
    {
        var domains = FetchDomains();
        if (domains.Count == 0) throw new Exception("fake-legal: no domains");
        var d = PickDomain(domains, domain);

        HttpResult resp;
        if (d == "imgui.de" || d == "pulsewebmenu.de")
        {
            // imgui-de / pulsewebmenu-de 用 POST 保根域
            var body = Json.Serialize(new Dictionary<string, object?>
            {
                ["username"] = RandomUsername(12),
                ["domain"] = d,
            });
            resp = Http.Post($"{Base}/api/inbox/custom", body, "application/json", Headers);
        }
        else
        {
            // 其余域名用 GET 创建
            resp = Http.Get($"{Base}/api/inbox/new?domain={WebUtility.UrlEncode(d)}", Headers);
        }
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null || !((data["success"] as JsonValue)?.GetValue<bool>() ?? false))
            throw new Exception("fake-legal: create inbox failed");
        var addr = Json.Str(data, "address").Trim();
        if (addr.Length == 0) throw new Exception("fake-legal: missing address");
        return new EmailInfo(channel, addr);
    }

    /// <summary>获取 fake-legal 邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var e = (email ?? "").Trim();
        if (e.Length == 0) throw new Exception("fake-legal: empty email");

        var resp = Http.Get($"{Base}/api/inbox/{WebUtility.UrlEncode(e)}", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data is null || !((data["success"] as JsonValue)?.GetValue<bool>() ?? false)) return result;
        if (data["emails"] is not JsonArray rows) return result;

        foreach (var raw in rows)
            if (raw is JsonObject ro) result.Add(Normalize.NormalizeEmail(Json.ToDict(ro), e));
        return result;
    }
}
