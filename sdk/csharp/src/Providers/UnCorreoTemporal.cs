using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// UnCorreoTemporal 渠道 — https://uncorreotemporal.com（API: /api/v1）。
/// 创建：POST /mailboxes 返回 address 与 session_token。
/// 收信：GET /mailboxes/{email}/messages?limit=50 列表 + 逐封 GET /mailboxes/{email}/messages/{id} 取详情，
///       鉴权头 X-Session-Token。
/// </summary>
public static class UnCorreoTemporal
{
    private const string ApiBase = "https://uncorreotemporal.com/api/v1";

    private static Dictionary<string, string> BaseHeaders() => new()
    {
        ["Accept"] = "application/json",
        ["Origin"] = "https://uncorreotemporal.com",
        ["Referer"] = "https://uncorreotemporal.com/",
        ["User-Agent"] = "Mozilla/5.0",
    };

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient) => new()
    {
        ["id"] = Json.Str(raw, "id"),
        ["from"] = Json.Str(raw, "from_address"),
        ["to"] = !string.IsNullOrEmpty(Json.Str(raw, "to_address")) ? Json.Str(raw, "to_address") : recipient,
        ["subject"] = Json.Str(raw, "subject"),
        ["text"] = Json.Str(raw, "body_text"),
        ["html"] = Json.Str(raw, "body_html"),
        ["date"] = Json.Str(raw, "received_at"),
        ["isRead"] = (raw["is_read"] as JsonValue)?.GetValue<bool>() ?? false,
        ["attachments"] = raw["attachments"] is JsonArray a ? Json.ToRaw(a) : null,
    };

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var h = BaseHeaders();
        h["Content-Type"] = "application/json";
        var resp = Http.Post($"{ApiBase}/mailboxes", null, "application/json", h);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var email = Json.Str(data, "address").Trim();
        var token = Json.Str(data, "session_token").Trim();
        if (string.IsNullOrEmpty(email) || !email.Contains('@') || string.IsNullOrEmpty(token))
            throw new Exception("uncorreotemporal: invalid mailbox response");
        long? expires = null;
        if (data!.TryGetPropertyValue("expires_at", out var ea) && ea is not null && long.TryParse(ea.ToString(), out var ev)) expires = ev;
        return new EmailInfo("uncorreotemporal", email, token, expires);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var sessionToken = (token ?? "").Trim();
        var address = (email ?? "").Trim();
        if (string.IsNullOrEmpty(sessionToken)) throw new Exception("uncorreotemporal: empty session token");
        if (string.IsNullOrEmpty(address)) throw new Exception("uncorreotemporal: empty email");

        var h = BaseHeaders();
        h["X-Session-Token"] = sessionToken;
        var resp = Http.Get($"{ApiBase}/mailboxes/{WebUtility.UrlEncode(address)}/messages?limit=50", h);
        resp.EnsureSuccess();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var r in rows)
        {
            if (r is not JsonObject row) continue;
            var messageId = Json.Str(row, "id").Trim();
            var detail = string.IsNullOrEmpty(messageId) ? null : FetchDetail(address, sessionToken, messageId);
            result.Add(Normalize.NormalizeEmail(Flatten(detail ?? row, address), address));
        }
        return result;
    }

    private static JsonObject? FetchDetail(string email, string token, string messageId)
    {
        try
        {
            var h = BaseHeaders();
            h["X-Session-Token"] = token;
            var resp = Http.Get($"{ApiBase}/mailboxes/{WebUtility.UrlEncode(email)}/messages/{WebUtility.UrlEncode(messageId)}", h);
            if (!resp.Ok) return null;
            return Json.Parse(resp.Body) as JsonObject;
        }
        catch { return null; }
    }
}
