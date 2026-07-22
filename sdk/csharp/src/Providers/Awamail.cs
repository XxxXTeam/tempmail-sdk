using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// awamail.com 渠道 — https://awamail.com/welcome
/// 需保存 awamail_session cookie 用于后续收信。
/// 创建：POST /change_mailbox（无 body），从 Set-Cookie 提取 session。
/// 收信：GET /get_emails 携带 Cookie。
/// </summary>
public static class Awamail
{
    private const string BaseUrl = "https://awamail.com/welcome";

    private static Dictionary<string, string> DefaultHeaders() => new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
        ["Accept"] = "application/json, text/javascript, */*; q=0.01",
        ["accept-language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["cache-control"] = "no-cache",
        ["dnt"] = "1",
        ["origin"] = "https://awamail.com",
        ["pragma"] = "no-cache",
        ["referer"] = "https://awamail.com/?lang=zh",
        ["sec-ch-ua"] = "\"Not(A:Brand\";v=\"8\", \"Chromium\";v=\"144\", \"Microsoft Edge\";v=\"144\"",
        ["sec-ch-ua-mobile"] = "?0",
        ["sec-ch-ua-platform"] = "\"Windows\"",
        ["sec-fetch-dest"] = "empty",
        ["sec-fetch-mode"] = "cors",
        ["sec-fetch-site"] = "same-origin",
    };

    /// <summary>创建 awamail 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var headers = DefaultHeaders();
        headers["Content-Length"] = "0";
        var resp = Http.Post($"{BaseUrl}/change_mailbox", null, null, headers);
        resp.EnsureSuccess();

        var sessionValue = ProviderHttpUtil.ExtractCookieValue(resp, "awamail_session");
        if (string.IsNullOrEmpty(sessionValue))
            throw new Exception("Failed to extract session cookie");
        var sessionCookie = $"awamail_session={sessionValue}";

        var data = Json.Parse(resp.Body) as JsonObject;
        var success = data?["success"]?.GetValue<bool>() ?? false;
        var inner = data?["data"] as JsonObject;
        if (!success || inner is null)
            throw new Exception("Failed to generate email");

        var email = Json.Str(inner, "email_address");
        long? expiresAt = null;
        if (inner["expired_at"] is JsonValue ev && ev.TryGetValue<long>(out var ea)) expiresAt = ea;

        return new EmailInfo("awamail", email, sessionCookie, expiresAt, Json.ToRaw(inner["created_at"]));
    }

    /// <summary>获取 awamail 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var headers = DefaultHeaders();
        headers["Cookie"] = token ?? "";
        headers["x-requested-with"] = "XMLHttpRequest";

        var resp = Http.Get($"{BaseUrl}/get_emails", headers);
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        var success = data?["success"]?.GetValue<bool>() ?? false;
        var inner = data?["data"] as JsonObject;
        if (!success || inner is null)
            throw new Exception("Failed to get emails");

        var result = new List<Email>();
        if (inner["emails"] is not JsonArray rows) return result;
        foreach (var row in rows)
            if (row is JsonObject) result.Add(Normalize.NormalizeEmail(Json.ToDict(row), email));
        return result;
    }
}
