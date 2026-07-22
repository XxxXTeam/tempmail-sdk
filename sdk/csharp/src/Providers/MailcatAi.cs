using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// mailcat-ai 渠道 — https://mailcat.ai（API https://api.mailcat.ai）
/// AI 驱动的临时邮箱，创建返回 Bearer token。
/// 创建：POST /mailboxes（无 body）→ {"data":{"email","token"}}。
/// 收信：GET /inbox 头 Authorization: Bearer {token} → {"data":[...]}。
/// token 存 Bearer token。
/// </summary>
public static class MailcatAi
{
    private const string BaseUrl = "https://api.mailcat.ai";

    private static Dictionary<string, string> Headers() => new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        ["Accept"] = "application/json",
    };

    /// <summary>创建 mailcat.ai 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Post($"{BaseUrl}/mailboxes", null, null, Headers());
        resp.EnsureSuccess();

        var body = Json.Parse(resp.Body) as JsonObject
            ?? throw new Exception($"mailcat-ai: 响应格式无效，响应: {resp.Body}");
        var data = body["data"] as JsonObject
            ?? throw new Exception($"mailcat-ai: 响应 data 格式无效，响应: {resp.Body}");

        var address = Json.Str(data, "email");
        var token = Json.Str(data, "token");
        if (address.Length == 0) throw new Exception($"mailcat-ai: 缺少邮箱地址，响应: {resp.Body}");
        if (token.Length == 0) throw new Exception($"mailcat-ai: 缺少认证令牌，响应: {resp.Body}");

        return new EmailInfo("mailcat-ai", address, token);
    }

    /// <summary>获取 mailcat.ai 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("mailcat-ai: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("mailcat-ai: 缺少邮箱地址");

        var headers = Headers();
        headers["Authorization"] = $"Bearer {token}";
        var resp = Http.Get($"{BaseUrl}/inbox", headers);
        if (resp.StatusCode == 404) return new List<Email>();
        resp.EnsureSuccess();

        var body = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (body?["data"] is not JsonArray messages || messages.Count == 0) return result;

        foreach (var node in messages)
        {
            if (node is not JsonObject msg) continue;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(msg, "id"),
                ["from"] = FirstNonEmpty(Json.Str(msg, "from"), Json.Str(msg, "sender")),
                ["to"] = addr,
                ["subject"] = Json.Str(msg, "subject"),
                ["text"] = FirstNonEmpty(Json.Str(msg, "text"), Json.Str(msg, "body_plain")),
                ["html"] = FirstNonEmpty(Json.Str(msg, "html"), Json.Str(msg, "body_html")),
                ["date"] = FirstNonEmpty(Json.Str(msg, "date"), Json.Str(msg, "received_at")),
                ["is_read"] = false,
            }, addr));
        }
        return result;
    }

    private static string FirstNonEmpty(params string[] values)
    {
        foreach (var v in values) if (!string.IsNullOrEmpty(v)) return v;
        return "";
    }
}
