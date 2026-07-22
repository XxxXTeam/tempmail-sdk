using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// tempmail.ing 渠道（api.tempmail.ing/api）。
/// 创建：POST /generate（duration 分钟）；收信：GET /emails/{email}，扁平化字段后归一。
/// </summary>
public static class Tempmail
{
    private const string Base = "https://api.tempmail.ing/api";

    private static Dictionary<string, string> Headers() => new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
        ["Referer"] = "https://tempmail.ing/",
    };

    /// <summary>创建临时邮箱，支持自定义有效期（分钟）</summary>
    public static EmailInfo Generate(int duration)
    {
        var payload = Json.Serialize(new Dictionary<string, object?> { ["duration"] = duration });
        var resp = Http.Post($"{Base}/generate", payload, "application/json", Headers());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null || (data["success"] as JsonValue)?.GetValue<bool>() != true)
            throw new Exception("Failed to generate email");
        var em = data["email"] as JsonObject ?? throw new Exception("Failed to generate email");
        return new EmailInfo("tempmail", Json.Str(em, "address"), createdAt: em["createdAt"]?.ToString());
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var resp = Http.Get($"{Base}/emails/{WebUtility.UrlEncode(email)}", Headers());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null || (data["success"] as JsonValue)?.GetValue<bool>() != true)
            throw new Exception("Failed to get emails");
        var rows = data["emails"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var raw in rows)
        {
            if (raw is not JsonObject ro) continue;
            var from = Json.Str(ro, "from_address");
            if (string.IsNullOrEmpty(from)) from = Json.Str(ro, "from");
            var html = Json.Str(ro, "content");
            if (string.IsNullOrEmpty(html)) html = Json.Str(ro, "html");
            var date = Json.Str(ro, "received_at");
            if (string.IsNullOrEmpty(date)) date = Json.Str(ro, "date");
            var flat = new Dictionary<string, object?>
            {
                ["id"] = Json.Str(ro, "id"),
                ["from"] = from,
                ["to"] = email,
                ["subject"] = Json.Str(ro, "subject"),
                ["text"] = Json.Str(ro, "text"),
                ["html"] = html,
                ["date"] = date,
            };
            result.Add(Normalize.NormalizeEmail(flat, email));
        }
        return result;
    }
}
