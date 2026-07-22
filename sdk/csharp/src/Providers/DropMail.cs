using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;
using System.Threading;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// dropmail 渠道 — https://dropmail.me GraphQL API。
/// 需要 af_ 认证令牌（可通过 DROPMAIL_AUTH_TOKEN 显式指定，否则自动申请并按生命周期缓存）。
/// 流程：申请/复用 af_ 令牌 → introduceSession 创建会话 → session(id) 查询邮件。
/// </summary>
public static class DropMail
{
    private const string Channel = "dropmail";
    private const string TokenGenerateUrl = "https://dropmail.me/api/token/generate";
    private const string TokenRenewUrl = "https://dropmail.me/api/token/renew";

    private const string CreateSessionQuery =
        "mutation {introduceSession {id, expiresAt, addresses{id, address}}}";

    private const string GetMailsQuery =
        "query ($id: ID!) {\n  session(id:$id) {\n    mails {\n      id, rawSize, fromAddr, toAddr, receivedAt,\n      text, headerFrom, headerSubject, html\n    }\n  }\n}";

    private static readonly Dictionary<string, string> TokenHeaders = new()
    {
        ["Accept"] = "application/json",
        ["Content-Type"] = "application/json",
        ["Origin"] = "https://dropmail.me",
        ["Referer"] = "https://dropmail.me/api/",
    };

    private const int AutoCacheSec = 50 * 60;
    private const int RenewBeforeSec = 10 * 60;

    private static readonly object Lock = new();
    private static string? _cachedToken;
    private static long _cachedExpiryTicks; // Environment.TickCount64 基准的过期毫秒

    private static string ExplicitAfToken()
    {
        var t = (Config.Get().DropmailAuthToken ?? "").Trim();
        if (t.Length > 0) return t;
        return (Environment.GetEnvironmentVariable("DROPMAIL_AUTH_TOKEN")
                ?? Environment.GetEnvironmentVariable("DROPMAIL_API_TOKEN") ?? "").Trim();
    }

    private static bool AutoTokenDisabled()
    {
        var v = (Environment.GetEnvironmentVariable("DROPMAIL_NO_AUTO_TOKEN") ?? "").Trim().ToLowerInvariant();
        return v is "1" or "true" or "yes";
    }

    private static JsonObject PostJson(string url, object payload)
    {
        var resp = Http.Post(url, Json.Serialize(payload), "application/json", TokenHeaders);
        resp.EnsureSuccess();
        return Json.Parse(resp.Body) as JsonObject ?? new JsonObject();
    }

    private static string FetchAfToken()
    {
        var body = PostJson(TokenGenerateUrl, new Dictionary<string, object?> { ["type"] = "af", ["lifetime"] = "1h" });
        var tok = Json.Str(body, "token").Trim();
        if (tok.Length == 0 || !tok.StartsWith("af_"))
            throw new Exception(Json.Str(body, "error") is { Length: > 0 } e ? e : "DropMail token/generate 未返回有效 af_ 令牌");
        return tok;
    }

    private static string ResolveAfToken()
    {
        var ex = ExplicitAfToken();
        if (ex.Length > 0) return ex;
        if (AutoTokenDisabled())
            throw new Exception("DropMail 已禁用自动令牌：请设置 DROPMAIL_AUTH_TOKEN 或 Config.DropmailAuthToken");

        lock (Lock)
        {
            var now = Environment.TickCount64;
            if (_cachedToken is not null && now < _cachedExpiryTicks - RenewBeforeSec * 1000L)
                return _cachedToken;

            var tok = FetchAfToken();
            _cachedToken = tok;
            _cachedExpiryTicks = now + AutoCacheSec * 1000L;
            return tok;
        }
    }

    private static string GraphqlUrl() => $"https://dropmail.me/api/graphql/{WebUtility.UrlEncode(ResolveAfToken())}";

    private static JsonObject GraphqlRequest(string query, IDictionary<string, string>? variables = null)
    {
        var form = new List<string> { $"query={WebUtility.UrlEncode(query)}" };
        if (variables is not null)
        {
            var varObj = new Dictionary<string, object?>();
            foreach (var kv in variables) varObj[kv.Key] = kv.Value;
            form.Add($"variables={WebUtility.UrlEncode(Json.Serialize(varObj))}");
        }
        var resp = Http.Post(GraphqlUrl(), string.Join("&", form), "application/x-www-form-urlencoded");
        resp.EnsureSuccess();
        var result = Json.Parse(resp.Body) as JsonObject;
        if (result?["errors"] is JsonArray errs && errs.Count > 0)
            throw new Exception($"GraphQL error: {Json.Str(errs[0] as JsonObject, "message")}");
        return result?["data"] as JsonObject ?? new JsonObject();
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var data = GraphqlRequest(CreateSessionQuery);
        var session = data["introduceSession"] as JsonObject;
        var addresses = session?["addresses"] as JsonArray;
        if (session is null || addresses is null || addresses.Count == 0)
            throw new Exception("Failed to generate email");

        var address = Json.Str(addresses[0] as JsonObject, "address");
        var id = Json.Str(session, "id");
        long? expiresAt = null;
        var expStr = Json.Str(session, "expiresAt");
        if (expStr.Length > 0 && DateTimeOffset.TryParse(expStr.Replace("Z", "+00:00"), out var dto))
            expiresAt = dto.ToUnixTimeMilliseconds();
        return new EmailInfo(Channel, address, id, expiresAt);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var data = GraphqlRequest(GetMailsQuery, new Dictionary<string, string> { ["id"] = token ?? "" });
        var mails = (data["session"] as JsonObject)?["mails"] as JsonArray;
        var result = new List<Email>();
        if (mails is null) return result;
        foreach (var m in mails)
        {
            if (m is not JsonObject mo) continue;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(mo, "id"),
                ["from"] = Json.Str(mo, "fromAddr"),
                ["to"] = string.IsNullOrEmpty(Json.Str(mo, "toAddr")) ? email : Json.Str(mo, "toAddr"),
                ["subject"] = Json.Str(mo, "headerSubject"),
                ["text"] = Json.Str(mo, "text"),
                ["html"] = Json.Str(mo, "html"),
                ["received_at"] = Json.Str(mo, "receivedAt"),
            }, email));
        }
        return result;
    }
}
