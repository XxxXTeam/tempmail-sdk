using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// GeniusGSM — https://tempmail.geniusgsm.com
/// 创建收件箱返回内部 ID；收件箱列表响应已包含 body_text / body_html。
/// </summary>
public static class GeniusGSM
{
    private const string ApiBase = "https://tempmail.geniusgsm.com/api";

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json",
        ["Content-Type"] = "application/json",
    };

    private static JsonArray? Rows(JsonNode? node)
    {
        if (node is JsonArray arr) return arr;
        if (node is JsonObject obj && obj["results"] is JsonArray results) return results;
        return null;
    }

    private static string FromValue(JsonObject raw)
    {
        var name = Json.Str(raw, "from_name");
        var address = Json.Str(raw, "from_email");
        return name.Length == 0 || address.Length == 0 ? address : $"{name} <{address}>";
    }

    /// <summary>选择激活域名并创建收件箱</summary>
    public static EmailInfo Generate(string channel)
    {
        var domainResp = Http.Get($"{ApiBase}/domains/", Headers());
        domainResp.EnsureSuccess();
        var domains = Rows(Json.Parse(domainResp.Body));
        var domainId = 0;
        if (domains is not null)
        {
            foreach (var item in domains)
            {
                if (item is not JsonObject obj) continue;
                if (obj["is_active"]?.GetValue<bool>() == true)
                {
                    domainId = obj["id"]!.GetValue<int>();
                    break;
                }
            }
        }
        if (domainId == 0) throw new Exception("geniusgsm: no active domain");

        var payload = $"{{\"domain_id\":{domainId},\"ttl\":3600}}";
        var resp = Http.Post($"{ApiBase}/inboxes/create_inbox/", payload, "application/json", Headers());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var id = data is null ? "" : Json.Str(data, "id").Trim();
        var email = data is null ? "" : Json.Str(data, "email").Trim();
        if (id.Length == 0 || email.Length == 0)
            throw new Exception("geniusgsm: incomplete creation response");
        return new EmailInfo(channel, email, id, createdAt: data?["created_at"]?.ToString());
    }

    /// <summary>获取 GeniusGSM 邮件，列表响应已包含完整正文</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("geniusgsm: empty inbox id");
        var resp = Http.Get($"{ApiBase}/inboxes/{Uri.EscapeDataString(token)}/emails/", Headers());
        resp.EnsureSuccess();
        var rows = Rows(Json.Parse(resp.Body));
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var item in rows)
        {
            if (item is not JsonObject io) continue;
            var flat = new Dictionary<string, object?>
            {
                ["id"] = Json.Str(io, "id"),
                ["from"] = FromValue(io),
                ["to"] = string.IsNullOrEmpty(Json.Str(io, "inbox_email")) ? email : Json.Str(io, "inbox_email"),
                ["subject"] = Json.Str(io, "subject"),
                ["text"] = Json.Str(io, "body_text"),
                ["html"] = Json.Str(io, "body_html"),
                ["date"] = Json.Str(io, "received_at"),
                ["read"] = Json.Str(io, "is_read"),
                ["attachments"] = Json.ToRaw(io["attachments"]),
            };
            result.Add(Normalize.NormalizeEmail(flat, email));
        }
        return result;
    }
}
