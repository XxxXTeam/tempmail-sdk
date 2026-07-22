using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// tempgo-email 渠道 — https://tempgo.email。
/// 创建：POST /api/generate（无 body、不发送 Content-Type），mailbox_id 作为 token。
/// 收信：GET /api/inbox?email={email}&mailbox_id={token}。
/// </summary>
public static class TempGoEmail
{
    private const string BaseUrl = "https://tempgo.email";
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        ["Accept"] = "application/json",
    };

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Post($"{BaseUrl}/api/generate", null, null, new Dictionary<string, string>(Headers));
        resp.EnsureSuccess();
        var body = Json.Parse(resp.Body) as JsonObject;
        var address = Json.Str(body, "email");
        var mailboxId = Json.Str(body, "mailbox_id");
        if (string.IsNullOrEmpty(address)) throw new Exception("tempgo-email: 缺少邮箱地址");
        if (string.IsNullOrEmpty(mailboxId)) throw new Exception("tempgo-email: 缺少 mailbox_id");
        return new EmailInfo("tempgo-email", address, mailboxId);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("tempgo-email: token 不能为空");
        var address = (email ?? "").Trim();
        if (string.IsNullOrEmpty(address)) throw new Exception("tempgo-email: 缺少邮箱地址");

        var url = $"{BaseUrl}/api/inbox?email={WebUtility.UrlEncode(address)}&mailbox_id={WebUtility.UrlEncode(token)}";
        var resp = Http.Get(url, new Dictionary<string, string>(Headers));
        if (resp.StatusCode == 404) return new List<Email>();
        resp.EnsureSuccess();

        var messages = (Json.Parse(resp.Body) as JsonObject)?["messages"] as JsonArray;
        var result = new List<Email>();
        if (messages is null) return result;

        foreach (var m in messages)
        {
            if (m is not JsonObject msg) continue;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(msg, "id"),
                ["from"] = !string.IsNullOrEmpty(Json.Str(msg, "from")) ? Json.Str(msg, "from") : Json.Str(msg, "sender"),
                ["to"] = address,
                ["subject"] = Json.Str(msg, "subject"),
                ["text"] = !string.IsNullOrEmpty(Json.Str(msg, "text")) ? Json.Str(msg, "text") : Json.Str(msg, "body_plain"),
                ["html"] = !string.IsNullOrEmpty(Json.Str(msg, "html")) ? Json.Str(msg, "html") : Json.Str(msg, "body_html"),
                ["date"] = !string.IsNullOrEmpty(Json.Str(msg, "date")) ? Json.Str(msg, "date") : Json.Str(msg, "received_at"),
            }, address));
        }
        return result;
    }
}
