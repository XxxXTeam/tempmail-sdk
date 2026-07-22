using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>GetNada 渠道及其众多域名别名（getnada.net/api）。</summary>
public static class Getnada
{
    private const string ApiBase = "https://getnada.net/api";
    private static readonly Random Rand = new();
    private static readonly Dictionary<string, string> HeadersJson = new() { ["Accept"] = "application/json" };

    private static string RandomLocal()
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder("sdk");
        for (var i = 0; i < 16; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    // 与 Python _DOMAIN_LABEL_RE 对齐：单个标签合法性校验
    private static readonly System.Text.RegularExpressions.Regex DomainLabelRe =
        new("^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$", System.Text.RegularExpressions.RegexOptions.IgnoreCase);

    // 与 Python _clean_domain 对齐：清洗并校验域名（长度、无 ".."、标签数与标签格式）
    private static string CleanDomain(string? value)
    {
        var domain = (value ?? "").Trim().ToLowerInvariant();
        if (domain.Length == 0 || domain.Length > 253 || domain.Contains("..")) return "";
        var labels = domain.Split('.');
        if (labels.Length < 2) return "";
        foreach (var label in labels)
            if (!DomainLabelRe.IsMatch(label)) return "";
        return domain;
    }

    private static string PickDomain(string? preferred)
    {
        var resp = Http.Get($"{ApiBase}/public/domains", HeadersJson);
        resp.EnsureSuccess();
        var arr = (Json.Parse(resp.Body) as JsonObject)?["domains"] as JsonArray;
        var domains = new List<string>();
        if (arr is not null)
            foreach (var d in arr)
            {
                var s = CleanDomain(d?.GetValue<string>());
                if (s.Length > 0) domains.Add(s);
            }
        var want = (preferred ?? "").Trim().TrimStart('@').ToLowerInvariant();
        if (want.Length > 0)
        {
            foreach (var d in domains) if (d == want) return d;
            throw new Exception($"getnada: domain not available: {want}");
        }
        foreach (var d in domains) if (d == "getnada.net") return d;
        if (domains.Count > 0) return domains[0];
        throw new Exception("getnada: no domain available");
    }

    /// <summary>创建邮箱。domain 指定域名别名，channel 为对外渠道标识。</summary>
    public static EmailInfo Generate(string? domain, string channel)
    {
        var selected = PickDomain(domain);
        var requested = $"{RandomLocal()}@{selected}";
        var payload = Json.Serialize(new Dictionary<string, object?> { ["email"] = requested });
        var resp = Http.Post($"{ApiBase}/inbox/open", payload, "application/json", HeadersJson);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject ?? throw new Exception("getnada: invalid open response");
        var token = Json.Str(data, "token");
        var email = Json.Str(data, "recipient");
        if (string.IsNullOrEmpty(email)) email = requested;
        if (string.IsNullOrEmpty(token) || !email.Contains('@'))
            throw new Exception("getnada: invalid open response");
        long? expiresAt = null;
        if (data["activeUntil"] is JsonValue av && av.TryGetValue<long>(out var au)) expiresAt = au;
        return new EmailInfo(channel, email, token, expiresAt);
    }

    public static List<Email> GetEmails(string? token, string email)
    {
        var auth = (token ?? "").Trim();
        if (string.IsNullOrEmpty(auth)) throw new Exception("getnada: empty token");
        var resp = Http.Get($"{ApiBase}/inbox/messages?token={WebUtility.UrlEncode(auth)}", HeadersJson);
        resp.EnsureSuccess();
        var rows = (Json.Parse(resp.Body) as JsonObject)?["messages"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var item in rows)
        {
            if (item is not JsonObject row) continue;
            var id = Json.Str(row, "id");
            JsonObject src = row;
            if (!string.IsNullOrEmpty(id))
            {
                try
                {
                    var dr = Http.Get($"{ApiBase}/inbox/message?id={WebUtility.UrlEncode(id)}&token={WebUtility.UrlEncode(auth)}", HeadersJson);
                    if (dr.Ok && (Json.Parse(dr.Body) as JsonObject)?["message"] is JsonObject det) src = det;
                }
                catch { /* 回退列表摘要 */ }
            }
            result.Add(Normalize.NormalizeEmail(Flatten(src, email), email));
        }
        return result;
    }

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient)
    {
        var dict = Json.ToDict(raw);
        dict["from"] = FirstNonEmpty(Json.Str(raw, "from_addr"), Json.Str(raw, "from"));
        var to = Json.Str(raw, "to");
        dict["to"] = string.IsNullOrEmpty(to) ? recipient : to;
        dict["text"] = FirstNonEmpty(Json.Str(raw, "text_plain"), Json.Str(raw, "text"));
        dict["html"] = FirstNonEmpty(Json.Str(raw, "html_sanitized"), Json.Str(raw, "html"));
        if (raw.ContainsKey("read_at")) dict["read"] = !string.IsNullOrEmpty(Json.Str(raw, "read_at"));
        return dict;
    }

    private static string FirstNonEmpty(params string[] values)
    {
        foreach (var v in values) if (!string.IsNullOrWhiteSpace(v)) return v;
        return "";
    }
}
