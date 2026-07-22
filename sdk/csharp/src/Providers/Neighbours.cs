using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Neighbours 渠道族（neighbours.sh）。公共收件箱，任意用户名即可收信，token 存邮箱地址本身。
/// 收信：GET /inbox/{addr} 取 uid 列表，再逐个 GET /inbox/{addr}/{uid} 补全正文。
/// 覆盖两个渠道：neighbours（从 /config/domains 取域名，本地 16 位随机名）、
/// neighbours-sh（固定域名 neighbours.sh，本地 10 位随机名）。
/// </summary>
public static class Neighbours
{
    private const string ApiBase = "https://neighbours.sh/api/v1";
    private const string FixedDomain = "neighbours.sh";
    private static readonly Random Rand = new();

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    };

    private static string RandomLocal(int n)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder("sdk");
        for (var i = 0; i < n; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static JsonObject? RequestJson(string path, bool allow404 = false)
    {
        var resp = Http.Get($"{ApiBase}{path}", Headers());
        if (allow404 && resp.StatusCode == 404) return null;
        resp.EnsureSuccess();
        return Json.Parse(resp.Body) as JsonObject;
    }

    /// <summary>从 from/to 结构中提取首个可用地址</summary>
    private static string FirstAddress(JsonNode? value)
    {
        switch (value)
        {
            case null:
                return "";
            case JsonValue v when v.TryGetValue<string>(out var s):
                return s.Trim();
            case JsonArray arr:
                foreach (var item in arr)
                {
                    var hit = FirstAddress(item);
                    if (hit.Length > 0) return hit;
                }
                return "";
            case JsonObject obj:
                var address = (obj["address"] as JsonValue)?.GetValue<string>()?.Trim() ?? "";
                if (address.Length > 0) return address;
                var text = (obj["text"] as JsonValue)?.GetValue<string>()?.Trim() ?? "";
                if (text.Contains('@')) return text;
                return FirstAddress(obj["value"]);
            default:
                return Json.NodeToString(value).Trim();
        }
    }

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient)
    {
        var text = Json.Str(raw, "text");
        if (string.IsNullOrEmpty(text)) text = Json.Str(raw, "snippet");
        var date = Json.Str(raw, "date");
        if (string.IsNullOrEmpty(date)) date = Json.Str(raw, "received_at");
        var to = FirstAddress(raw["to"]);
        return new Dictionary<string, object?>
        {
            ["id"] = Json.Str(raw, "uid"),
            ["from"] = FirstAddress(raw["from"]),
            ["to"] = string.IsNullOrEmpty(to) ? recipient : to,
            ["subject"] = Json.Str(raw, "subject"),
            ["text"] = text,
            ["html"] = Json.Str(raw, "html"),
            ["date"] = date,
            ["is_read"] = false,
            ["attachments"] = Json.ToRaw(raw["attachments"]),
        };
    }

    private static List<string> ListDomains()
    {
        var data = RequestJson("/config/domains");
        var domains = new List<string>();
        if (data?["data"] is JsonObject nested && nested["domains"] is JsonArray items)
            foreach (var item in items)
            {
                var s = Json.NodeToString(item).Trim().ToLowerInvariant();
                if (s.Length > 0) domains.Add(s);
            }
        if (domains.Count == 0) throw new Exception("neighbours: domain list is empty");
        return domains;
    }

    private static JsonObject? FetchDetail(string address, string uid)
    {
        var data = RequestJson($"/inbox/{WebUtility.UrlEncode(address)}/{WebUtility.UrlEncode(uid)}", allow404: true);
        return data?["data"] as JsonObject;
    }

    // ---- neighbours ----

    /// <summary>创建 neighbours 邮箱：从域名列表挑选（或校验指定域名），本地 16 位随机名</summary>
    public static EmailInfo Generate(string? domain)
    {
        var domains = ListDomains();
        string selected;
        var wanted = (domain ?? "").Trim().ToLowerInvariant();
        if (wanted.Length > 0)
        {
            if (!domains.Contains(wanted)) throw new Exception($"neighbours: unsupported domain {domain}");
            selected = wanted;
        }
        else
        {
            selected = domains[Rand.Next(domains.Count)];
        }
        var email = $"{RandomLocal(16)}@{selected}";
        return new EmailInfo("neighbours", email, email);
    }

    /// <summary>获取 neighbours 邮件列表（详情缺失时回退列表项）</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("neighbours: empty email");
        var data = RequestJson($"/inbox/{WebUtility.UrlEncode(address)}", allow404: true);
        var result = new List<Email>();
        if (data?["data"] is not JsonArray rows) return result;
        foreach (var item in rows)
        {
            if (item is not JsonObject ro) continue;
            var uid = Json.Str(ro, "uid").Trim();
            var detail = uid.Length > 0 ? FetchDetail(address, uid) : null;
            result.Add(Normalize.NormalizeEmail(Flatten(detail ?? ro, address), address));
        }
        return result;
    }

    // ---- neighbours-sh ----

    /// <summary>创建 neighbours-sh 邮箱：固定域名，本地 10 位随机名</summary>
    public static EmailInfo GenerateSh()
    {
        var email = $"{RandomLocal(10)}@{FixedDomain}";
        return new EmailInfo("neighbours-sh", email, email);
    }

    /// <summary>获取 neighbours-sh 邮件列表（详情缺失时跳过该封）</summary>
    public static List<Email> GetEmailsSh(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("neighbours-sh: 缺少邮箱地址");
        var data = RequestJson($"/inbox/{WebUtility.UrlEncode(address)}", allow404: true);
        var result = new List<Email>();
        if (data?["data"] is not JsonArray rows) return result;
        foreach (var item in rows)
        {
            if (item is not JsonObject ro) continue;
            var uid = Json.Str(ro, "uid").Trim();
            if (uid.Length == 0) continue;
            var detail = FetchDetail(address, uid);
            if (detail is null) continue;
            result.Add(Normalize.NormalizeEmail(Flatten(detail, address), address));
        }
        return result;
    }
}
