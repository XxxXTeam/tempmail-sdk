using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// mail.tm 协议家族实现（mail.tm / duckmail 共用同一套 Hydra REST API，仅 baseUrl 不同）。
/// 流程：获取域名 → 随机邮箱/密码 → 创建账号 → 换取 Bearer Token。
/// </summary>
public static class MailTm
{
    private static readonly Random Rand = new();

    private static string RandomString(int length)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static Dictionary<string, string> BaseHeaders(string baseUrl) => new()
    {
        ["Accept"] = "application/json",
        ["Origin"] = OriginOf(baseUrl),
    };

    private static string OriginOf(string baseUrl)
    {
        try { var u = new Uri(baseUrl); return $"{u.Scheme}://{u.Host}"; }
        catch { return baseUrl; }
    }

    private static List<string> GetDomains(string baseUrl)
    {
        var resp = Http.Get($"{baseUrl}/domains", BaseHeaders(baseUrl));
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        var members = data as JsonArray ?? (data as JsonObject)?["hydra:member"] as JsonArray;
        var domains = new List<string>();
        if (members is not null)
            foreach (var m in members)
                if (m is JsonObject o && (o["isActive"]?.GetValue<bool>() ?? false))
                    domains.Add(o["domain"]!.GetValue<string>());
        return domains;
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate(string channel, string baseUrl)
    {
        var domains = GetDomains(baseUrl);
        if (domains.Count == 0) throw new Exception("No available domains");

        var address = $"{RandomString(12)}@{domains[Rand.Next(domains.Count)]}";
        var password = RandomString(16);
        var payload = Json.Serialize(new Dictionary<string, object?> { ["address"] = address, ["password"] = password });

        var headers = BaseHeaders(baseUrl);
        var accResp = Http.Post($"{baseUrl}/accounts", payload, "application/ld+json", headers);
        accResp.EnsureSuccess();
        var account = Json.Parse(accResp.Body);

        var tokResp = Http.Post($"{baseUrl}/token", payload, "application/json", headers);
        tokResp.EnsureSuccess();
        var token = Json.Str(Json.Parse(tokResp.Body), "token");

        return new EmailInfo(channel, address, token, createdAt: Json.Str(account, "createdAt"));
    }

    /// <summary>获取邮件列表，逐封拉取详情</summary>
    public static List<Email> GetEmails(string baseUrl, string? token, string email)
    {
        var headers = BaseHeaders(baseUrl);
        headers["Authorization"] = $"Bearer {token}";

        var resp = Http.Get($"{baseUrl}/messages", headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        var messages = data as JsonArray ?? (data as JsonObject)?["hydra:member"] as JsonArray;

        var result = new List<Email>();
        if (messages is null) return result;

        foreach (var msg in messages)
        {
            if (msg is not JsonObject mo) continue;
            var id = Json.Str(mo, "id");
            JsonNode? detail = mo;
            if (!string.IsNullOrEmpty(id))
            {
                try
                {
                    var dr = Http.Get($"{baseUrl}/messages/{id}", headers);
                    if (dr.Ok) detail = Json.Parse(dr.Body);
                }
                catch { /* 回退到列表摘要 */ }
            }
            result.Add(Normalize.NormalizeEmail(Flatten(detail as JsonObject ?? mo, email, baseUrl), email));
        }
        return result;
    }

    private static Dictionary<string, object?> Flatten(JsonObject msg, string recipient, string baseUrl)
    {
        var from = "";
        if (msg["from"] is JsonObject fo) from = fo["address"]?.GetValue<string>() ?? "";
        else if (msg["from"] is JsonValue fv && fv.TryGetValue<string>(out var fs)) from = fs;

        var to = recipient;
        if (msg["to"] is JsonArray ta && ta.Count > 0 && ta[0] is JsonObject t0)
            to = t0["address"]?.GetValue<string>() ?? recipient;

        var html = "";
        if (msg["html"] is JsonArray ha)
            foreach (var h in ha) html += h?.GetValue<string>() ?? "";
        else html = Json.Str(msg, "html");

        return new Dictionary<string, object?>
        {
            ["id"] = Json.Str(msg, "id"),
            ["from"] = from,
            ["to"] = to,
            ["subject"] = Json.Str(msg, "subject"),
            ["text"] = Json.Str(msg, "text"),
            ["html"] = html,
            ["createdAt"] = Json.Str(msg, "createdAt"),
            ["seen"] = (msg["seen"] as JsonValue)?.GetValue<bool>() ?? false,
        };
    }
}
