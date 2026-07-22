using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// TempMail Pro 渠道 — https://tempmailpro.us（API: /api/v1）。
/// 创建：POST /mailbox/create 返回 data.address 与 data.token。
/// 收信：GET /mailbox/{token}/emails 列表 + 逐封 GET /mailbox/{token}/emails/{id} 取详情。
/// </summary>
public static class TempMailPro
{
    private const string ApiBase = "https://tempmailpro.us/api/v1";
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0",
    };

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient) => new()
    {
        ["id"] = !string.IsNullOrEmpty(Json.Str(raw, "id")) ? Json.Str(raw, "id") : Json.Str(raw, "message_id"),
        ["from"] = !string.IsNullOrEmpty(Json.Str(raw, "from_address")) ? Json.Str(raw, "from_address") : Json.Str(raw, "from_name"),
        ["to"] = recipient,
        ["subject"] = Json.Str(raw, "subject"),
        ["text"] = Json.Str(raw, "body_text"),
        ["html"] = Json.Str(raw, "body_html"),
        ["date"] = Json.Str(raw, "received_at"),
        ["attachments"] = raw["attachments"] is JsonArray a ? Json.ToRaw(a) : null,
    };

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var h = new Dictionary<string, string>(Headers) { ["Content-Type"] = "application/json" };
        var resp = Http.Post($"{ApiBase}/mailbox/create", "{}", "application/json", h);
        resp.EnsureSuccess();
        var box = (Json.Parse(resp.Body) as JsonObject)?["data"] as JsonObject;
        if (box is null) throw new Exception("tempmailpro: invalid mailbox response");
        var email = Json.Str(box, "address").Trim();
        var token = Json.Str(box, "token").Trim();
        if (string.IsNullOrEmpty(email) || !email.Contains('@') || string.IsNullOrEmpty(token))
            throw new Exception("tempmailpro: invalid mailbox response");
        long? expires = null;
        if (box.TryGetPropertyValue("expires_at", out var ea) && ea is not null && long.TryParse(ea.ToString(), out var ev)) expires = ev;
        return new EmailInfo("tempmailpro", email, token, expires, box["created_at"]?.ToString());
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var mailboxToken = (token ?? "").Trim();
        var address = (email ?? "").Trim();
        if (string.IsNullOrEmpty(mailboxToken)) throw new Exception("tempmailpro: empty token");
        if (string.IsNullOrEmpty(address)) throw new Exception("tempmailpro: empty email");

        var resp = Http.Get($"{ApiBase}/mailbox/{WebUtility.UrlEncode(mailboxToken)}/emails", new Dictionary<string, string>(Headers));
        resp.EnsureSuccess();
        var rows = (Json.Parse(resp.Body) as JsonObject)?["data"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var r in rows)
        {
            if (r is not JsonObject item) continue;
            var messageId = Json.Str(item, "id").Trim();
            var detail = string.IsNullOrEmpty(messageId) ? null : FetchDetail(mailboxToken, messageId);
            result.Add(Normalize.NormalizeEmail(Flatten(detail ?? item, address), address));
        }
        return result;
    }

    private static JsonObject? FetchDetail(string token, string messageId)
    {
        var resp = Http.Get($"{ApiBase}/mailbox/{WebUtility.UrlEncode(token)}/emails/{WebUtility.UrlEncode(messageId)}",
            new Dictionary<string, string>(Headers));
        if (!resp.Ok) return null;
        return (Json.Parse(resp.Body) as JsonObject)?["data"] as JsonObject;
    }
}
