using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>temp-mail.io 渠道（api.internal.temp-mail.io/api/v3）。</summary>
public static class TempMailIo
{
    private const string BaseUrl = "https://api.internal.temp-mail.io/api/v3";

    private static Dictionary<string, string> BuildHeaders() => new()
    {
        ["Application-Name"] = "web",
        ["Application-Version"] = "4.0.0",
        ["X-CORS-Header"] = "1",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0",
        ["Origin"] = "https://temp-mail.io",
        ["Referer"] = "https://temp-mail.io/",
    };

    public static EmailInfo Generate()
    {
        var payload = Json.Serialize(new Dictionary<string, object?> { ["min_name_length"] = 10, ["max_name_length"] = 10 });
        var resp = Http.Post($"{BaseUrl}/email/new", payload, "application/json", BuildHeaders());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        var email = Json.Str(data, "email");
        var token = Json.Str(data, "token");
        if (string.IsNullOrEmpty(email) || string.IsNullOrEmpty(token))
            throw new Exception("temp-mail-io: invalid generate response");
        return new EmailInfo("temp-mail-io", email, token);
    }

    public static List<Email> GetEmails(string email)
    {
        var resp = Http.Get($"{BaseUrl}/email/{WebUtility.UrlEncode(email)}/messages", BuildHeaders());
        resp.EnsureSuccess();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var raw in rows)
            if (raw is JsonObject) result.Add(Normalize.NormalizeEmail(Json.ToDict(raw), email));
        return result;
    }
}

/// <summary>mail.cx 匿名 Web API 渠道（mail.cx/v1）。ddker.com 复用。</summary>
public static class MailCx
{
    private const string BaseUrl = "https://mail.cx";
    private static readonly Random Rand = new();

    private static string RandomString(int len)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder();
        for (var i = 0; i < len; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static Dictionary<string, string> BuildHeaders(string clientId) => new()
    {
        ["Accept"] = "application/json",
        ["Origin"] = BaseUrl,
        ["Referer"] = BaseUrl + "/",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        ["X-Client-ID"] = clientId,
    };

    public static EmailInfo Generate(string channel, string? preferredDomain)
    {
        var clientId = "tempmail-sdk-" + RandomString(16);
        var resp = Http.Get($"{BaseUrl}/v1/config", BuildHeaders(clientId));
        resp.EnsureSuccess();
        var config = Json.Parse(resp.Body) as JsonObject;

        var domain = PickDomain(config, preferredDomain);
        return new EmailInfo(channel, $"{RandomString(12)}@{domain}", clientId);
    }

    private static string PickDomain(JsonObject? config, string? preferred)
    {
        var items = config?["system_domains"] as JsonArray;
        var domains = new List<string>();
        if (items is not null)
            foreach (var it in items)
                if (it is JsonObject o)
                {
                    var d = (o["domain"]?.GetValue<string>() ?? "").Trim();
                    if (d.Length > 0) domains.Add(d);
                }
        if (domains.Count == 0) throw new Exception("mail-cx: no system domains");

        var want = (preferred ?? "").Trim().TrimStart('@').ToLowerInvariant();
        if (want.Length > 0)
            foreach (var d in domains)
                if (d.ToLowerInvariant() == want) return d;

        if (items is not null)
            foreach (var it in items)
                if (it is JsonObject o && (o["default"]?.GetValue<bool>() ?? false))
                {
                    var d = (o["domain"]?.GetValue<string>() ?? "").Trim();
                    if (d.Length > 0) return d;
                }
        return domains[0];
    }

    public static List<Email> GetEmails(string? clientId, string email)
    {
        var resp = Http.Get($"{BaseUrl}/v1/inbox/{WebUtility.UrlEncode(email)}", BuildHeaders(clientId ?? ""));
        if (resp.StatusCode == 204) return new List<Email>();
        resp.EnsureSuccess();
        var rows = (Json.Parse(resp.Body) as JsonObject)?["emails"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var row in rows)
        {
            if (row is not JsonObject ro) { result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>(), email)); continue; }
            var raw = new Dictionary<string, object?>
            {
                ["id"] = Json.Str(ro, "id"),
                ["from"] = FirstNonEmpty(Json.Str(ro, "from_email"), Json.Str(ro, "from_name")),
                ["to"] = email,
                ["subject"] = Json.Str(ro, "subject"),
                ["text"] = Json.Str(ro, "preview_text"),
                ["created_at"] = Json.Str(ro, "created_at"),
            };
            var id = Json.Str(ro, "id");
            if (!string.IsNullOrEmpty(id))
            {
                try
                {
                    var dr = Http.Get($"{BaseUrl}/v1/email/{WebUtility.UrlEncode(id)}", BuildHeaders(clientId ?? ""));
                    if (dr.Ok && Json.Parse(dr.Body) is JsonObject det)
                        raw = new Dictionary<string, object?>
                        {
                            ["id"] = Json.Str(det, "id"),
                            ["from"] = FirstNonEmpty(Json.Str(det, "from_email"), Json.Str(det, "from_name")),
                            ["to"] = email,
                            ["subject"] = Json.Str(det, "subject"),
                            ["text"] = FirstNonEmpty(Json.Str(det, "text_body"), Json.Str(det, "preview_text")),
                            ["html"] = Json.Str(det, "html_body"),
                            ["created_at"] = Json.Str(det, "created_at"),
                        };
                }
                catch { /* 回退列表摘要 */ }
            }
            result.Add(Normalize.NormalizeEmail(raw, email));
        }
        return result;
    }

    private static string FirstNonEmpty(params string[] values)
    {
        foreach (var v in values) if (!string.IsNullOrWhiteSpace(v)) return v.Trim();
        return "";
    }
}
