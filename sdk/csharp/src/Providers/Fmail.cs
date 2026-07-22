using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>fmail 渠道（fmail.men）。GET JSON API，逐封拉取正文。</summary>
public static class Fmail
{
    private const string BaseUrl = "https://fmail.men";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    };

    private static JsonObject FetchJson(string path)
    {
        var resp = Http.Get($"{BaseUrl}{path}", Headers);
        resp.EnsureSuccess();
        return Json.Parse(resp.Body) as JsonObject ?? new JsonObject();
    }

    private static string? NormalizeDomain(string? domain)
    {
        var value = (domain ?? "").Trim().TrimStart('@');
        return value.Length > 0 ? value : null;
    }

    private static Dictionary<string, object?> FlattenMessage(JsonObject raw, string recipient)
    {
        return new Dictionary<string, object?>
        {
            ["id"] = Pick(raw, "id", "uid", "token"),
            ["from"] = Pick(raw, "sender", "from"),
            ["to"] = PickOr(raw, recipient, "recipient", "to"),
            ["subject"] = Pick(raw, "subject"),
            ["text"] = Pick(raw, "body_text", "text", "snippet"),
            ["html"] = Pick(raw, "body_html", "html"),
            ["date"] = Pick(raw, "received_at", "timestamp", "date"),
            ["is_read"] = (raw["is_read"] as JsonValue)?.GetValue<bool>() ?? false,
        };
    }

    /// <summary>创建临时邮箱（可指定域名）</summary>
    public static EmailInfo Generate(string? domain = null)
    {
        var selected = NormalizeDomain(domain);
        var path = "/v1/random";
        if (selected is not null) path = $"{path}?domain={WebUtility.UrlEncode(selected)}";
        var data = FetchJson(path);
        var email = (data["address"]?.GetValue<string>() ?? "").Trim();
        if (email.Length == 0)
        {
            var username = (data["username"]?.GetValue<string>() ?? "").Trim();
            var dom = (data["domain"]?.GetValue<string>() ?? "").Trim();
            if (username.Length > 0 && dom.Length > 0) email = $"{username}@{dom}";
        }
        if (email.Length == 0 || !email.Contains('@')) throw new Exception("fmail: invalid random response");
        return new EmailInfo("fmail", email);
    }

    /// <summary>获取邮件列表，逐封补全正文</summary>
    public static List<Email> GetEmails(string email)
    {
        var value = (email ?? "").Trim();
        var idx = value.IndexOf('@');
        if (idx <= 0 || idx == value.Length - 1) throw new Exception("fmail: invalid email");
        var local = value[..idx];
        var domain = value[(idx + 1)..];

        var data = FetchJson($"/v1/inbox/{WebUtility.UrlEncode(local)}?domain={WebUtility.UrlEncode(domain)}&limit=50");
        var result = new List<Email>();
        if (data["emails"] is not JsonArray rows) return result;

        foreach (var r in rows)
        {
            if (r is not JsonObject row) continue;
            var token = Pick(row, "token", "id").Trim();
            if (token.Length == 0)
            {
                result.Add(Normalize.NormalizeEmail(FlattenMessage(row, value), value));
                continue;
            }
            try
            {
                var detail = FetchJson($"/v1/email/{WebUtility.UrlEncode(token)}");
                var nested = detail["email"] as JsonObject ?? detail;
                result.Add(Normalize.NormalizeEmail(FlattenMessage(nested, value), value));
            }
            catch
            {
                result.Add(Normalize.NormalizeEmail(FlattenMessage(row, value), value));
            }
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

    private static string PickOr(JsonObject o, string fallback, params string[] keys)
    {
        var v = Pick(o, keys);
        return v.Length > 0 ? v : fallback;
    }
}
