using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// ThrowawayMail 渠道 — https://throwawaymail.app（API: /api）。
/// 创建：POST /mailboxes 返回 mailbox_id 与 address。
/// 收信：GET /mailboxes/{id}/messages 列表 + 逐封 GET /mailboxes/{id}/messages/{mid} 取详情。
/// </summary>
public static class ThrowawayMail
{
    private const string ApiBase = "https://throwawaymail.app/api";
    private static readonly Dictionary<string, string> Headers = new() { ["Accept"] = "application/json" };

    private static Dictionary<string, object?> MessageRaw(JsonObject raw, string recipient)
    {
        var to = recipient;
        if (raw["to_addresses"] is JsonArray ta && ta.Count > 0)
            to = Json.NodeToString(ta[0]);
        return new Dictionary<string, object?>
        {
            ["id"] = Json.Str(raw, "message_id"),
            ["messageId"] = Json.Str(raw, "message_id"),
            ["from_address"] = Json.Str(raw, "from_address"),
            ["fromName"] = Json.Str(raw, "from_name"),
            ["to"] = to,
            ["subject"] = Json.Str(raw, "subject"),
            ["received_at"] = Json.Str(raw, "received_at"),
            ["read"] = (raw["read"] as JsonValue)?.GetValue<bool>() ?? false,
            ["text"] = Json.Str(raw, "text"),
            ["html"] = Json.Str(raw, "html"),
        };
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Post($"{ApiBase}/mailboxes", null, null, new Dictionary<string, string>(Headers));
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var mailboxId = Json.Str(data, "mailbox_id").Trim();
        var email = Json.Str(data, "address").Trim();
        if (string.IsNullOrEmpty(mailboxId) || string.IsNullOrEmpty(email) || !email.Contains('@'))
            throw new Exception("throwawaymail: invalid mailbox response");
        long? expires = null;
        if (data!.TryGetPropertyValue("expires_at", out var ea) && ea is not null && long.TryParse(ea.ToString(), out var ev)) expires = ev;
        return new EmailInfo("throwawaymail", email, mailboxId, expires, data["created_at"]?.ToString());
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var mailboxId = (token ?? "").Trim();
        var address = (email ?? "").Trim();
        if (string.IsNullOrEmpty(mailboxId)) throw new Exception("throwawaymail: empty mailbox id");
        if (string.IsNullOrEmpty(address)) throw new Exception("throwawaymail: empty email");

        var resp = Http.Get($"{ApiBase}/mailboxes/{WebUtility.UrlEncode(mailboxId)}/messages", new Dictionary<string, string>(Headers));
        resp.EnsureSuccess();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var r in rows)
        {
            if (r is not JsonObject item) continue;
            var messageId = Json.Str(item, "message_id").Trim();
            var detail = string.IsNullOrEmpty(messageId) ? null : FetchMessage(mailboxId, messageId);
            result.Add(Normalize.NormalizeEmail(MessageRaw(detail ?? item, address), address));
        }
        return result;
    }

    private static JsonObject? FetchMessage(string mailboxId, string messageId)
    {
        var resp = Http.Get($"{ApiBase}/mailboxes/{WebUtility.UrlEncode(mailboxId)}/messages/{WebUtility.UrlEncode(messageId)}",
            new Dictionary<string, string>(Headers));
        if (!resp.Ok) return null;
        return Json.Parse(resp.Body) as JsonObject;
    }
}
