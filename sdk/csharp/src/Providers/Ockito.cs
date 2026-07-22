using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// ockito 渠道（ockito.com/web-api）。
/// 创建：POST /gtoken 取 access/refresh token → GET /email 取地址；token 打包 access+refresh 为 JSON。
/// 收信：GET /retrieve/{addr}/emails 列表，逐封 GET 详情；401 时用 refresh token 换新 access。
/// </summary>
public static class Ockito
{
    private const string Base = "https://ockito.com/web-api";

    private static Dictionary<string, string> DefaultHeaders() => new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    };

    /// <summary>发起请求并解析 JSON；非 2xx 抛出含状态码的异常</summary>
    private static (int status, JsonObject data) FetchJson(string path, string method = "GET",
        Dictionary<string, string>? headers = null, string? payload = null)
    {
        var reqHeaders = DefaultHeaders();
        if (headers is not null)
            foreach (var kv in headers) reqHeaders[kv.Key] = kv.Value;

        HttpResult resp = method == "POST"
            ? Http.Post($"{Base}{path}", payload ?? "{}", "application/json", reqHeaders)
            : Http.Get($"{Base}{path}", reqHeaders);

        var text = resp.Body ?? "";
        JsonNode? node;
        try { node = string.IsNullOrEmpty(text) ? null : Json.Parse(text); }
        catch { throw new Exception($"ockito invalid JSON: {path} HTTP {resp.StatusCode}"); }
        if (!resp.Ok) throw new Exception($"ockito http {resp.StatusCode}");
        return (resp.StatusCode, node as JsonObject ?? new JsonObject());
    }

    private static string PackToken(string access, string refresh) =>
        Json.Serialize(new Dictionary<string, object?> { ["access_token"] = access, ["refresh_token"] = refresh });

    private static (string access, string refresh) UnpackToken(string? token)
    {
        var value = (token ?? "").Trim();
        if (value.Length == 0 || !value.StartsWith("{")) throw new Exception("ockito: invalid session token");
        if (Json.Parse(value) is not JsonObject data) throw new Exception("ockito: invalid session token");
        var access = Json.Str(data, "access_token").Trim();
        var refresh = Json.Str(data, "refresh_token").Trim();
        if (access.Length == 0 || refresh.Length == 0) throw new Exception("ockito: invalid session token");
        return (access, refresh);
    }

    private static string RefreshAccessToken(string refresh)
    {
        var (status, data) = FetchJson("/grefresh", "POST",
            new Dictionary<string, string> { ["Authorization"] = $"Bearer {refresh}", ["X-PASSTHROUGH"] = "Y" });
        if (status is < 200 or >= 300) throw new Exception($"ockito grefresh http {status}");
        var access = Json.Str(data, "access_token").Trim();
        if (access.Length == 0) throw new Exception("ockito: invalid refresh response");
        return access;
    }

    private static JsonObject FetchBearerJson(string path, string access, string refresh)
    {
        try
        {
            var (_, data) = FetchJson(path, headers: new Dictionary<string, string> { ["Authorization"] = $"Bearer {access}" });
            return data;
        }
        catch (Exception exc) when (exc.Message.Contains("http 401"))
        {
            var refreshed = RefreshAccessToken(refresh);
            var (_, data) = FetchJson(path, headers: new Dictionary<string, string> { ["Authorization"] = $"Bearer {refreshed}" });
            return data;
        }
    }

    private static Dictionary<string, object?> FlattenInboxRow(JsonObject raw, string recipient) => new()
    {
        ["id"] = Json.Str(raw, "uid"),
        ["from"] = Json.Str(raw, "sender"),
        ["to"] = recipient,
        ["subject"] = Json.Str(raw, "subject"),
        ["text"] = Json.Str(raw, "snippet"),
        ["html"] = Json.Str(raw, "html"),
        ["date"] = Json.Str(raw, "timestamp"),
        ["is_read"] = false,
    };

    private static Dictionary<string, object?> FlattenMessage(JsonObject raw, string recipient)
    {
        var obj = raw["obj"] as JsonObject ?? raw;
        string Pick(params string[] keys)
        {
            foreach (var k in keys)
            {
                var v = Json.Str(obj, k);
                if (!string.IsNullOrEmpty(v)) return v;
            }
            return "";
        }
        var to = Pick("to", "To");
        return new Dictionary<string, object?>
        {
            ["id"] = Json.Str(raw, "uid"),
            ["from"] = Pick("sender_email", "SenderEmail", "from_", "From", "from", "sender_name", "SenderName"),
            ["to"] = string.IsNullOrEmpty(to) ? recipient : to,
            ["subject"] = Pick("subject", "Subject"),
            ["text"] = Json.Str(obj, "text"),
            ["html"] = Json.Str(obj, "html"),
            ["date"] = Pick("date", "Date"),
            ["is_read"] = false,
        };
    }

    /// <summary>创建 ockito 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var (_, login) = FetchJson("/gtoken", "POST", payload: "{}");
        var access = Json.Str(login, "access_token").Trim();
        var refresh = Json.Str(login, "refresh_token").Trim();
        if (access.Length == 0 || refresh.Length == 0) throw new Exception("ockito: invalid token response");

        var (_, emailData) = FetchJson("/email", headers: new Dictionary<string, string> { ["Authorization"] = $"Bearer {access}" });
        var email = "";
        if (emailData["email"] is JsonValue ev && ev.TryGetValue<string>(out var es)) email = es.Trim();
        else if (emailData["email"] is JsonObject eo) email = Json.Str(eo, "email").Trim();
        if (email.Length == 0 || !email.Contains('@')) throw new Exception("ockito: invalid email response");

        return new EmailInfo("ockito", email, PackToken(access, refresh));
    }

    /// <summary>获取 ockito 邮件列表（详情失败回退列表摘要）</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var (access, refresh) = UnpackToken(token);
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("ockito: empty email");

        var data = FetchBearerJson($"/retrieve/{WebUtility.UrlEncode(address)}/emails", access, refresh);
        var result = new List<Email>();
        if (data["inbox"] is not JsonArray rows) return result;
        foreach (var row in rows)
        {
            if (row is not JsonObject ro) continue;
            var uid = Json.Str(ro, "uid").Trim();
            if (uid.Length == 0)
            {
                result.Add(Normalize.NormalizeEmail(FlattenInboxRow(ro, address), address));
                continue;
            }
            try
            {
                var detail = FetchBearerJson($"/retrieve/{WebUtility.UrlEncode(address)}/{WebUtility.UrlEncode(uid)}", access, refresh);
                result.Add(Normalize.NormalizeEmail(FlattenMessage(detail, address), address));
            }
            catch
            {
                result.Add(Normalize.NormalizeEmail(FlattenInboxRow(ro, address), address));
            }
        }
        return result;
    }
}
