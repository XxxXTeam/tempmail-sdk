using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Temporam 渠道（temporam.com/api）。
/// 创建：GET /api/domains 取域名，本地随机 sdk+18 位地址，token 存邮箱地址本身。
/// 收信：GET /api/emails?email=&amp;limit=50 列表，逐封 GET /api/emails/{id} 补全详情。
/// </summary>
public static class Temporam
{
    private const string Origin = "https://temporam.com";
    private static readonly Random Rand = new();

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    };

    private static string RandomLocal(int length = 18)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder("sdk");
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static JsonObject? GetJson(string path)
    {
        var resp = Http.Get($"{Origin}{path}", Headers());
        resp.EnsureSuccess();
        return Json.Parse(resp.Body) as JsonObject;
    }

    private static List<string> Domains()
    {
        var data = GetJson("/api/domains");
        var domains = new List<string>();
        if (data?["data"] is JsonArray arr)
            foreach (var item in arr)
                if (item is JsonObject o)
                {
                    var d = Json.Str(o, "domain").Trim();
                    if (d.Length > 0) domains.Add(d);
                }
        if (domains.Count == 0) throw new Exception("temporam: domain list is empty");
        return domains;
    }

    private static Email NormalizeRow(JsonObject raw, string email)
    {
        var flat = Json.ToDict(raw);
        var id = Json.Str(raw, "id");
        if (string.IsNullOrEmpty(id)) id = Json.Str(raw, "uuid");
        var from = Json.Str(raw, "fromEmail");
        if (string.IsNullOrEmpty(from)) from = Json.Str(raw, "from");
        var to = Json.Str(raw, "toEmail");
        if (string.IsNullOrEmpty(to)) to = Json.Str(raw, "to");
        if (string.IsNullOrEmpty(to)) to = email;
        var date = Json.Str(raw, "createdAt");
        if (string.IsNullOrEmpty(date)) date = Json.Str(raw, "created_at");
        if (string.IsNullOrEmpty(date)) date = Json.Str(raw, "date");
        flat["id"] = id;
        flat["from"] = from;
        flat["to"] = to;
        flat["date"] = date;
        return Normalize.NormalizeEmail(flat, email);
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate(string? domain)
    {
        var domains = Domains();
        var selected = domains[Rand.Next(domains.Count)];
        if (!string.IsNullOrEmpty(domain))
        {
            if (!domains.Contains(domain)) throw new Exception($"temporam: unsupported domain {domain}");
            selected = domain;
        }
        var email = $"{RandomLocal()}@{selected}";
        return new EmailInfo("temporam", email, email);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var recipient = !string.IsNullOrEmpty(token) ? token! : email;
        if (string.IsNullOrEmpty(recipient)) throw new Exception("temporam: missing email");
        var data = GetJson($"/api/emails?email={WebUtility.UrlEncode(recipient)}&limit=50");
        var result = new List<Email>();
        if (data?["data"] is not JsonArray rows) return result;
        foreach (var row in rows)
        {
            if (row is not JsonObject ro) continue;
            var raw = ro;
            var msgId = Json.Str(ro, "id");
            if (string.IsNullOrEmpty(msgId)) msgId = Json.Str(ro, "uuid");
            if (!string.IsNullOrEmpty(msgId))
            {
                try
                {
                    var detail = GetJson($"/api/emails/{WebUtility.UrlEncode(msgId)}");
                    if (detail?["data"] is JsonObject det) raw = det;
                }
                catch { raw = ro; }
            }
            result.Add(NormalizeRow(raw, recipient));
        }
        return result;
    }
}
