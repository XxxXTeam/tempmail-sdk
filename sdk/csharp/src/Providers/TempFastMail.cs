using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// TempFastMail 渠道 — https://tempfastmail.com。
/// 创建：POST /api/email-box 返回 email 与 uuid（uuid 作为 token）。
/// 收信：GET /api/email-box/{uuid}/emails 列表 + 逐封 GET /api/email-box/{uuid}/email/{id} 取详情。
/// </summary>
public static class TempFastMail
{
    private const string BaseUrl = "https://tempfastmail.com";
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0",
    };

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient) => new()
    {
        ["id"] = Json.Str(raw, "uuid"),
        ["from"] = Json.Str(raw, "from"),
        ["to"] = !string.IsNullOrEmpty(Json.Str(raw, "real_to")) ? Json.Str(raw, "real_to") : recipient,
        ["subject"] = Json.Str(raw, "subject"),
        ["html"] = Json.Str(raw, "html"),
        ["date"] = Json.Str(raw, "received_at"),
        ["attachments"] = raw["attachments"] is JsonArray a ? Json.ToRaw(a) : null,
    };

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Post($"{BaseUrl}/api/email-box", null, null, new Dictionary<string, string>(Headers));
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        var email = Json.Str(data, "email").Trim();
        var uuid = Json.Str(data, "uuid").Trim();
        if (string.IsNullOrEmpty(email) || !email.Contains('@') || string.IsNullOrEmpty(uuid))
            throw new System.Exception("tempfastmail: invalid create response");
        return new EmailInfo("tempfastmail", email, uuid);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var uuid = (token ?? "").Trim();
        var address = (email ?? "").Trim();
        if (string.IsNullOrEmpty(uuid)) throw new System.Exception("tempfastmail: empty token");
        if (string.IsNullOrEmpty(address)) throw new System.Exception("tempfastmail: empty email");

        var resp = Http.Get($"{BaseUrl}/api/email-box/{WebUtility.UrlEncode(uuid)}/emails", new Dictionary<string, string>(Headers));
        resp.EnsureSuccess();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var r in rows)
        {
            var raw = r as JsonObject ?? new JsonObject();
            var messageId = Json.Str(raw, "uuid").Trim();
            if (!string.IsNullOrEmpty(messageId))
            {
                var dr = Http.Get($"{BaseUrl}/api/email-box/{WebUtility.UrlEncode(uuid)}/email/{WebUtility.UrlEncode(messageId)}",
                    new Dictionary<string, string>(Headers));
                if (dr.Ok && Json.Parse(dr.Body) is JsonObject detail) raw = detail;
            }
            result.Add(Normalize.NormalizeEmail(Flatten(raw, address), address));
        }
        return result;
    }
}
