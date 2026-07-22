using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// mail.chatgpt.org.uk 渠道 — https://mail.chatgpt.org.uk/api
/// 创建：GET /domains/public 取域名 → 本地随机用户名 → POST /inbox-token 取 JWT + gm_sid。
/// 收信：GET /emails?email=xxx，头 x-inbox-token + Cookie gm_sid，401/403 时重建收件箱重试。
/// token 存 JSON {"gmSid","inbox"}。
/// </summary>
public static class ChatgptOrgUk
{
    private const string BaseUrl = "https://mail.chatgpt.org.uk/api";
    private static readonly Random Rand = new();

    private static Dictionary<string, string> DefaultHeaders() => new()
    {
        ["User-Agent"] = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36 Edg/150.0.0.0",
        ["Accept"] = "*/*",
        ["Referer"] = "https://mail.chatgpt.org.uk/zh/",
        ["Origin"] = "https://mail.chatgpt.org.uk",
        ["DNT"] = "1",
        ["sec-fetch-dest"] = "empty",
        ["sec-fetch-mode"] = "cors",
        ["sec-fetch-site"] = "same-origin",
    };

    private static string RandomUsername(int length)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static List<string> FetchDomains()
    {
        var resp = Http.Get($"{BaseUrl}/domains/public", DefaultHeaders());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (!(data?["success"]?.GetValue<bool>() ?? false))
            throw new Exception("chatgpt-org-uk: 获取域名失败");
        var domains = new List<string>();
        if ((data["data"] as JsonObject)?["domains"] is JsonArray arr)
            foreach (var node in arr)
                if (node is JsonObject o && (o["is_active"]?.GetValue<int>() ?? 0) == 1)
                {
                    var d = Json.Str(o, "domain_name");
                    if (!string.IsNullOrEmpty(d)) domains.Add(d);
                }
        return domains;
    }

    private static (string token, string gmSid) CreateInbox(string email)
    {
        var payload = Json.Serialize(new Dictionary<string, object?> { ["email"] = email });
        var headers = DefaultHeaders();
        headers["Content-Type"] = "application/json";
        var resp = Http.Post($"{BaseUrl}/inbox-token", payload, "application/json", headers);
        resp.EnsureSuccess();
        var gmSid = ProviderHttpUtil.ExtractCookieValue(resp, "gm_sid");
        var data = Json.Parse(resp.Body) as JsonObject;
        var token = Json.Str(data?["auth"] as JsonObject, "token");
        if (string.IsNullOrEmpty(token))
            throw new Exception("chatgpt-org-uk: inbox-token 响应缺少 token");
        return (token, gmSid);
    }

    /// <summary>创建 chatgpt-org-uk 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var domains = FetchDomains();
        if (domains.Count == 0) throw new Exception("chatgpt-org-uk: 无可用域名");
        var domain = domains[Rand.Next(domains.Count)];
        var email = $"{RandomUsername(10)}@{domain}";
        var (inboxToken, gmSid) = CreateInbox(email);
        var packed = Json.Serialize(new Dictionary<string, object?> { ["gmSid"] = gmSid, ["inbox"] = inboxToken });
        return new EmailInfo("chatgpt-org-uk", email, packed);
    }

    private static (string gmSid, string inbox) ParsePacked(string packed)
    {
        var t = packed.Trim();
        if (t.StartsWith('{') && Json.Parse(t) is JsonObject o)
            return (Json.Str(o, "gmSid"), Json.Str(o, "inbox"));
        return ("", packed);
    }

    private static List<Email> FetchEmails(string inboxToken, string gmSid, string email)
    {
        var headers = DefaultHeaders();
        headers["x-inbox-token"] = inboxToken;
        if (!string.IsNullOrEmpty(gmSid)) headers["Cookie"] = $"gm_sid={gmSid}";
        var resp = Http.Get($"{BaseUrl}/emails?email={WebUtility.UrlEncode(email)}", headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (!(data?["success"]?.GetValue<bool>() ?? false))
            throw new Exception("chatgpt-org-uk: 获取邮件失败");
        var result = new List<Email>();
        if ((data["data"] as JsonObject)?["emails"] is JsonArray rows)
            foreach (var row in rows)
                if (row is JsonObject) result.Add(Normalize.NormalizeEmail(Json.ToDict(row), email));
        return result;
    }

    /// <summary>获取 chatgpt-org-uk 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("chatgpt-org-uk: token 不能为空");
        var (gmSid, inbox) = ParsePacked(token);
        if (string.IsNullOrEmpty(gmSid))
        {
            var created = CreateInbox(email);
            inbox = created.token;
            gmSid = created.gmSid;
        }
        try
        {
            return FetchEmails(inbox, gmSid, email);
        }
        catch (Exception ex) when (ex.Message.Contains("401") || ex.Message.Contains("403"))
        {
            var (newInbox, newSid) = CreateInbox(email);
            return FetchEmails(newInbox, newSid, email);
        }
    }
}
