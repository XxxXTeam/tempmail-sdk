using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>MailToYou / m2u 渠道（api.m2u.io）。token 打包 token+viewToken。</summary>
public static class M2u
{
    private const string ApiBase = "https://api.m2u.io";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    };

    private static string PackToken(string token, string viewToken)
        => Json.Serialize(new Dictionary<string, object?> { ["token"] = token, ["viewToken"] = viewToken });

    private static (string token, string viewToken) UnpackToken(string? token)
    {
        var value = (token ?? "").Trim();
        if (value.Length == 0) return ("", "");
        if (value.StartsWith("{"))
        {
            var data = Json.Parse(value) as JsonObject;
            if (data is not null)
                return ((data["token"]?.GetValue<string>() ?? "").Trim(), (data["viewToken"]?.GetValue<string>() ?? "").Trim());
            return ("", "");
        }
        return (value, "");
    }

    private static Dictionary<string, object?> FlattenMessage(JsonObject raw, string recipient)
    {
        return new Dictionary<string, object?>
        {
            ["id"] = Pick(raw, "id", "message_id"),
            ["from"] = Pick(raw, "from_addr", "from_address", "from"),
            ["to"] = recipient,
            ["subject"] = Pick(raw, "subject"),
            ["text"] = Pick(raw, "text_body", "body_text", "text"),
            ["html"] = Pick(raw, "html_body", "body_html", "html"),
            ["date"] = Pick(raw, "received_at", "created_at"),
            ["is_read"] = (raw["is_read"] as JsonValue)?.GetValue<bool>()
                          ?? (raw["isRead"] as JsonValue)?.GetValue<bool>()
                          ?? (raw["seen"] as JsonValue)?.GetValue<bool>() ?? false,
        };
    }

    /// <summary>创建临时邮箱：POST /v1/mailboxes/auto</summary>
    public static EmailInfo Generate()
    {
        var headers = new Dictionary<string, string>(Headers) { ["Content-Type"] = "application/json" };
        var resp = Http.Post($"{ApiBase}/v1/mailboxes/auto", "{}", "application/json", headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data?["mailbox"] is not JsonObject mailbox) throw new Exception("m2u: invalid mailbox response");
        var localPart = (mailbox["local_part"]?.GetValue<string>() ?? "").Trim();
        var domain = (mailbox["domain"]?.GetValue<string>() ?? "").Trim();
        var token = (mailbox["token"]?.GetValue<string>() ?? "").Trim();
        var viewToken = (mailbox["view_token"]?.GetValue<string>() ?? "").Trim();
        var email = localPart.Length > 0 && domain.Length > 0 ? $"{localPart}@{domain}" : "";
        if (email.Length == 0 || token.Length == 0 || viewToken.Length == 0)
            throw new Exception("m2u: invalid mailbox response");

        long? expiresAt = null;
        if (mailbox["expires_at"] is JsonValue ev)
        {
            if (ev.TryGetValue<long>(out var l)) expiresAt = l;
            else if (ev.TryGetValue<double>(out var d)) expiresAt = (long)d;
        }
        return new EmailInfo("m2u", email, PackToken(token, viewToken), expiresAt);
    }

    private static JsonObject? FetchDetail(string token, string viewToken, string messageId)
    {
        var resp = Http.Get(
            $"{ApiBase}/v1/mailboxes/{WebUtility.UrlEncode(token)}/messages/{WebUtility.UrlEncode(messageId)}?view={WebUtility.UrlEncode(viewToken)}",
            Headers);
        if (!resp.Ok) return null;
        var data = Json.Parse(resp.Body) as JsonObject;
        return data?["message"] as JsonObject;
    }

    /// <summary>获取邮件列表，逐封补全详情</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var (mailboxToken, viewToken) = UnpackToken(token);
        var address = (email ?? "").Trim();
        if (mailboxToken.Length == 0) throw new Exception("m2u: missing token");
        if (viewToken.Length == 0) throw new Exception("m2u: missing view token");
        if (address.Length == 0) throw new Exception("m2u: empty email");

        var resp = Http.Get(
            $"{ApiBase}/v1/mailboxes/{WebUtility.UrlEncode(mailboxToken)}/messages?view={WebUtility.UrlEncode(viewToken)}",
            Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data?["messages"] is not JsonArray rows) return result;

        foreach (var item in rows)
        {
            if (item is not JsonObject msg) continue;
            var messageId = Pick(msg, "id", "message_id").Trim();
            var detail = messageId.Length > 0 ? FetchDetail(mailboxToken, viewToken, messageId) : null;
            result.Add(Normalize.NormalizeEmail(FlattenMessage(detail ?? msg, address), address));
        }
        return result;
    }

    private static string Pick(JsonObject o, params string[] keys)
    {
        foreach (var k in keys)
        {
            if (o[k] is JsonNode n)
            {
                var s = Json.NodeToString(n);
                if (!string.IsNullOrEmpty(s)) return s;
            }
        }
        return "";
    }
}
