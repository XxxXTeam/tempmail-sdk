using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Emailnator 渠道 — https://www.emailnator.com
/// 需 XSRF-TOKEN Session 认证。
/// 创建：GET / 取 XSRF-TOKEN cookie → POST /generate-email（body email 选项数组）。
/// 收信：POST /message-list（body email），对每封 POST 带 messageID 取 HTML 正文。
/// token 存 JSON {"xsrfToken","cookies"}。
/// </summary>
public static class Emailnator
{
    private const string BaseUrl = "https://www.emailnator.com";

    private static readonly string[] EmailOptions = { "plusGmail", "dotGmail", "googleMail" };

    private static Dictionary<string, string> DefaultHeaders() => new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36",
        ["Accept"] = "application/json",
        ["Content-Type"] = "application/json",
        ["X-Requested-With"] = "XMLHttpRequest",
    };

    private static (string xsrf, string cookie) InitSession()
    {
        var resp = Http.Get(BaseUrl, new Dictionary<string, string> { ["User-Agent"] = DefaultHeaders()["User-Agent"] });
        resp.EnsureSuccess();
        var xsrf = ProviderHttpUtil.ExtractCookieValue(resp, "XSRF-TOKEN");
        if (string.IsNullOrEmpty(xsrf)) throw new Exception("Failed to extract XSRF-TOKEN");
        xsrf = WebUtility.UrlDecode(xsrf);
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);
        return (xsrf, cookie);
    }

    /// <summary>创建 emailnator 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var (xsrf, cookie) = InitSession();
        var headers = DefaultHeaders();
        headers["X-XSRF-TOKEN"] = xsrf;
        headers["Cookie"] = cookie;

        var payload = Json.Serialize(new Dictionary<string, object?> { ["email"] = EmailOptions });
        var resp = Http.Post($"{BaseUrl}/generate-email", payload, "application/json", headers);
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        var list = data?["email"] as JsonArray;
        if (list is null || list.Count == 0)
            throw new Exception("Failed to generate email: empty response");
        var email = list[0]!.GetValue<string>();

        var token = Json.Serialize(new Dictionary<string, object?> { ["xsrfToken"] = xsrf, ["cookies"] = cookie });
        return new EmailInfo("emailnator", email, token);
    }

    /// <summary>获取 emailnator 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var session = Json.Parse(token ?? "") as JsonObject ?? throw new Exception("emailnator: 无效 token");
        var xsrf = Json.Str(session, "xsrfToken");
        var cookie = Json.Str(session, "cookies");

        var headers = DefaultHeaders();
        headers["X-XSRF-TOKEN"] = xsrf;
        headers["Cookie"] = cookie;

        var payload = Json.Serialize(new Dictionary<string, object?> { ["email"] = email });
        var resp = Http.Post($"{BaseUrl}/message-list", payload, "application/json", headers);
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data?["messageData"] is not JsonArray rows) return result;

        foreach (var node in rows)
        {
            if (node is not JsonObject msg) continue;
            var msgId = Json.Str(msg, "messageID");
            if (msgId.StartsWith("ADS", StringComparison.Ordinal)) continue;

            var html = "";
            if (!string.IsNullOrEmpty(msgId))
            {
                try
                {
                    var detailPayload = Json.Serialize(new Dictionary<string, object?> { ["email"] = email, ["messageID"] = msgId });
                    var detail = Http.Post($"{BaseUrl}/message-list", detailPayload, "application/json", headers);
                    detail.EnsureSuccess();
                    var detailNode = Json.Parse(detail.Body);
                    html = detailNode is JsonValue dv && dv.TryGetValue<string>(out var s) ? s
                        : detailNode is null ? detail.Body : detailNode.ToJsonString();
                }
                catch { html = ""; }
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = msgId,
                ["from"] = Json.Str(msg, "from"),
                ["to"] = email,
                ["subject"] = Json.Str(msg, "subject"),
                ["text"] = "",
                ["html"] = html,
                ["date"] = Json.Str(msg, "time"),
                ["isRead"] = false,
            }, email));
        }
        return result;
    }
}
