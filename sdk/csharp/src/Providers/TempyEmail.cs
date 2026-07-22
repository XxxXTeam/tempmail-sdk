using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Tempy Email 渠道（tempy.email/api/v1）。
/// 创建：POST /mailbox（可选 domain）；收信：GET /mailbox/{email}/messages，多候选字段扁平化。
/// </summary>
public static class TempyEmail
{
    private const string ApiBase = "https://tempy.email/api/v1";

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    };

    private static string Pick(JsonObject raw, params string[] keys)
    {
        foreach (var k in keys)
        {
            var v = Json.Str(raw, k);
            if (!string.IsNullOrEmpty(v)) return v;
        }
        return "";
    }

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient)
    {
        var to = Json.Str(raw, "to");
        return new Dictionary<string, object?>
        {
            ["id"] = Pick(raw, "id", "messageId", "message_id", "mail_id"),
            ["from"] = Pick(raw, "from", "sender", "from_addr", "from_address"),
            ["to"] = string.IsNullOrEmpty(to) ? recipient : to,
            ["subject"] = Pick(raw, "subject", "mail_title"),
            ["text"] = Pick(raw, "text", "body_text", "text_body", "body"),
            ["html"] = Pick(raw, "html", "body_html", "html_body"),
            ["date"] = Pick(raw, "date", "received_at", "created_at"),
            ["is_read"] = (raw["is_read"] as JsonValue)?.GetValue<bool>()
                ?? (raw["isRead"] as JsonValue)?.GetValue<bool>()
                ?? (raw["seen"] as JsonValue)?.GetValue<bool>() ?? false,
            ["attachments"] = Json.ToRaw(raw["attachments"]),
        };
    }

    /// <summary>创建邮箱</summary>
    public static EmailInfo Generate(string? domain)
    {
        var body = new Dictionary<string, object?>();
        var wanted = (domain ?? "").Trim();
        if (wanted.Length > 0) body["domain"] = wanted;
        var headers = Headers();
        var resp = Http.Post($"{ApiBase}/mailbox", Json.Serialize(body), "application/json", headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject ?? throw new Exception("tempy-email: invalid create response");
        var email = Json.Str(data, "email").Trim();
        if (email.Length == 0) throw new Exception("tempy-email: invalid create response");
        return new EmailInfo("tempy-email", email);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("tempy-email: empty email");
        var resp = Http.Get($"{ApiBase}/mailbox/{WebUtility.UrlEncode(address)}/messages", Headers());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        var rows = (data as JsonObject)?["messages"] as JsonArray ?? data as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var item in rows)
            if (item is JsonObject ro) result.Add(Normalize.NormalizeEmail(Flatten(ro, address), address));
        return result;
    }
}
