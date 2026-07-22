using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// OpenInbox 渠道 — https://openinbox.io（API https://api.openinbox.io/api）
/// 创建：POST /inbox（无 body）→ {"id","email","expiresAt","createdAt"}。
/// 收信：GET /emails/inbox/{id}?page=1&limit=50 取列表 → GET /emails/{id} 取详情。
/// token 存 inbox id。
/// </summary>
public static class OpenInbox
{
    private const string ApiBase = "https://api.openinbox.io/api";

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Origin"] = "https://openinbox.io",
        ["Referer"] = "https://openinbox.io/",
        ["User-Agent"] = "Mozilla/5.0",
    };

    /// <summary>创建 openinbox 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var headers = Headers();
        headers["Content-Type"] = "application/json";
        var resp = Http.Post($"{ApiBase}/inbox", null, "application/json", headers);
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject
            ?? throw new Exception("openinbox: invalid mailbox response");
        var inboxId = Json.Str(data, "id").Trim();
        var email = Json.Str(data, "email").Trim();
        if (inboxId.Length == 0 || email.Length == 0 || !email.Contains('@'))
            throw new Exception("openinbox: invalid mailbox response");

        var expiresAt = data["expiresAt"]?.GetValue<long?>();
        return new EmailInfo("openinbox", email, inboxId, expiresAt, Json.ToRaw(data["createdAt"]));
    }

    private static JsonObject? FetchDetail(string messageId)
    {
        try
        {
            var resp = Http.Get($"{ApiBase}/emails/{WebUtility.UrlEncode(messageId)}", Headers());
            if (!resp.Ok) return null;
            return Json.Parse(resp.Body) as JsonObject;
        }
        catch { return null; }
    }

    /// <summary>获取 openinbox 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var inboxId = (token ?? "").Trim();
        var addr = (email ?? "").Trim();
        if (inboxId.Length == 0) throw new Exception("openinbox: empty inbox id");
        if (addr.Length == 0) throw new Exception("openinbox: empty email");

        var resp = Http.Get($"{ApiBase}/emails/inbox/{WebUtility.UrlEncode(inboxId)}?page=1&limit=50", Headers());
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data?["emails"] is not JsonArray rows) return result;

        foreach (var node in rows)
        {
            if (node is not JsonObject row) continue;
            var messageId = Json.Str(row, "id").Trim();
            var detail = messageId.Length > 0 ? FetchDetail(messageId) : null;
            var src = detail ?? row;

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(src, "id"),
                ["from"] = Json.Str(src, "from"),
                ["to"] = addr,
                ["subject"] = Json.Str(src, "subject"),
                ["text"] = Json.Str(src, "textBody"),
                ["html"] = Json.Str(src, "htmlBody"),
                ["receivedAt"] = Json.Str(src, "receivedAt"),
                ["isRead"] = src["isRead"] is JsonValue v && v.TryGetValue<bool>(out var b) && b,
            }, addr));
        }
        return result;
    }
}
