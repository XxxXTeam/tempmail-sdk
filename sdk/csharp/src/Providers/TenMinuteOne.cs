using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// 10minutemail.one 渠道家族。
/// 从 SSR 页面 __NUXT_DATA__ 中解析 mailServiceToken（JWT）与 emailDomains，
/// 收信通过 Bearer JWT 调用 web.10minutemail.one/api/v1/mailbox/{email}。
/// xghff.com / oqqaj.com 等为别名，仅指定固定邮箱域名。
/// </summary>
public static class TenMinuteOne
{
    private const string Channel = "10minute-one";
    private const string SiteOrigin = "https://10minutemail.one";
    private const string ApiBase = "https://web.10minutemail.one/api/v1";
    private static readonly string[] KnownDomains =
        { "xghff.com", "oqqaj.com", "psovv.com", "dbwot.com", "ygwpr.com", "imxwe.com" };
    private const string DefaultUa =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

    private static readonly Random Rand = new();
    private static readonly Regex NuxtDataRe = new(
        "<script[^>]*\\bid=\"__NUXT_DATA__\"[^>]*>([\\s\\S]*?)</script>", RegexOptions.IgnoreCase);
    private static readonly Regex JwtRe = new("^[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+$", RegexOptions.Compiled);

    private static string EncMailboxEmail(string email)
    {
        var at = email.IndexOf('@');
        if (at >= 0)
        {
            var local = email[..at];
            var dom = email[(at + 1)..];
            return $"{WebUtility.UrlEncode(local)}%40{WebUtility.UrlEncode(dom)}";
        }
        return WebUtility.UrlEncode(email);
    }

    private static JsonArray ParseNuxtArray(string html)
    {
        var m = NuxtDataRe.Match(html);
        if (!m.Success) throw new Exception("10minute-one: __NUXT_DATA__ not found");
        return Json.Parse(m.Groups[1].Value.Trim()) as JsonArray
            ?? throw new Exception("10minute-one: invalid __NUXT_DATA__");
    }

    private static JsonNode? ResolveRef(JsonArray arr, JsonNode? value, int depth = 0)
    {
        if (depth > 64) return value;
        if (value is JsonValue v && v.TryGetValue<bool>(out _)) return value;
        if (value is JsonValue vi && vi.TryGetValue<long>(out var idx) && idx >= 0 && idx < arr.Count)
            return ResolveRef(arr, arr[(int)idx], depth + 1);
        return value;
    }

    private static string ParseMailServiceToken(JsonArray arr)
    {
        var n = Math.Min(arr.Count, 48);
        for (var i = 0; i < n; i++)
        {
            if (arr[i] is JsonObject el && el.TryGetPropertyValue("mailServiceToken", out var tv))
            {
                var t = ResolveRef(arr, tv);
                if (t is JsonValue jv && jv.TryGetValue<string>(out var s) && JwtRe.IsMatch(s)) return s;
            }
        }
        foreach (var e in arr)
        {
            if (e is JsonObject el && el.TryGetPropertyValue("mailServiceToken", out var tv))
            {
                var t = ResolveRef(arr, tv);
                if (t is JsonValue jv && jv.TryGetValue<string>(out var s) && JwtRe.IsMatch(s)) return s;
            }
        }
        foreach (var e in arr)
            if (e is JsonValue jv && jv.TryGetValue<string>(out var s) && JwtRe.IsMatch(s)) return s;
        throw new Exception("10minute-one: mailServiceToken not found");
    }

    // 从 SSR HTML 中解析形如 field:"[...]" 的转义 JSON 数组
    private static List<string> ParseQuotedJsonArray(string html, string field)
    {
        var key = $"{field}:\"";
        var start = html.IndexOf(key, StringComparison.Ordinal);
        if (start < 0) return new List<string>();
        var sliceStart = start + key.Length;
        if (sliceStart >= html.Length || html[sliceStart] != '[') return new List<string>();
        var depth = 0;
        var j = sliceStart;
        while (j < html.Length)
        {
            var c = html[j];
            if (c == '[') depth++;
            else if (c == ']')
            {
                depth--;
                if (depth == 0) { j++; break; }
            }
            j++;
        }
        var raw = html[sliceStart..j].Replace("\\\"", "\"");
        var arr = Json.Parse(raw) as JsonArray;
        var result = new List<string>();
        if (arr is null) return result;
        foreach (var it in arr)
            if (it is JsonValue v && v.TryGetValue<string>(out var s)) result.Add(s);
        return result;
    }

    private static string PickLocale(string? domain)
    {
        var s = (domain ?? "").Trim();
        if (s.Length == 0) return "zh";
        if (s.Contains('.') && !s.Contains('/')) return "zh";
        return s;
    }

    private static string PickMailboxDomain(string? hint, List<string> available)
    {
        if (available.Count == 0) throw new Exception("10minute-one: no email domains");
        if (!string.IsNullOrEmpty(hint))
        {
            var h = hint.Trim().ToLowerInvariant();
            if (h.Contains('.'))
                foreach (var d in available)
                    if (d.ToLowerInvariant() == h) return d;
        }
        return available[Rand.Next(available.Count)];
    }

    private static string RandomLocal(int n)
    {
        const string alphabet = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < n; i++) sb.Append(alphabet[Rand.Next(alphabet.Length)]);
        return sb.ToString();
    }

    private static long? JwtExpMs(string token)
    {
        var parts = token.Split('.');
        if (parts.Length < 2) return null;
        try
        {
            var b = parts[1].Replace('-', '+').Replace('_', '/');
            b = b.PadRight(b.Length + (4 - b.Length % 4) % 4, '=');
            var payloadBytes = Convert.FromBase64String(b);
            var payload = Json.Parse(Encoding.UTF8.GetString(payloadBytes)) as JsonObject;
            if (payload?["exp"] is JsonValue v && v.TryGetValue<double>(out var exp))
                return (long)(exp * 1000);
        }
        catch { /* 忽略解析失败 */ }
        return null;
    }

    private static Dictionary<string, string> ApiHeaders(string tok) => new()
    {
        ["Accept"] = "*/*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Authorization"] = $"Bearer {tok}",
        ["Cache-Control"] = "no-cache",
        ["Content-Type"] = "application/json",
        ["DNT"] = "1",
        ["Origin"] = SiteOrigin,
        ["Pragma"] = "no-cache",
        ["Referer"] = $"{SiteOrigin}/",
        ["User-Agent"] = DefaultUa,
        ["X-Request-ID"] = RandomHex(16),
        ["X-Timestamp"] = DateTimeOffset.UtcNow.ToUnixTimeSeconds().ToString(),
    };

    private static string RandomHex(int bytes)
    {
        var buf = new byte[bytes];
        Rand.NextBytes(buf);
        var sb = new StringBuilder();
        foreach (var b in buf) sb.Append(b.ToString("x2"));
        return sb.ToString();
    }

    private static bool ItemNeedsDetail(JsonObject m)
    {
        if (Json.Str(m, "id").Trim().Length == 0) return false;
        var body = Json.Str(m, "text");
        if (body.Length == 0) body = Json.Str(m, "body");
        if (body.Length == 0) body = Json.Str(m, "html");
        if (body.Length == 0) body = Json.Str(m, "mail_text");
        return body.Trim().Length == 0;
    }

    /// <summary>创建 10minute-one 临时邮箱（domain 可为固定邮箱域名或语言标识；channel 为渠道标识）</summary>
    public static EmailInfo Generate(string? domain, string? channel = null)
    {
        var selectedChannel = string.IsNullOrWhiteSpace(channel) ? Channel : channel.Trim();
        var loc = PickLocale(domain);
        var pageUrl = $"{SiteOrigin}/{loc}";
        var r = Http.Get(pageUrl, new Dictionary<string, string>
        {
            ["User-Agent"] = DefaultUa,
            ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
            ["Cache-Control"] = "no-cache",
            ["Pragma"] = "no-cache",
            ["DNT"] = "1",
            ["Referer"] = pageUrl,
        });
        r.EnsureSuccess();
        var html = r.Body;
        var arr = ParseNuxtArray(html);
        var token = ParseMailServiceToken(arr);

        // 合并页面 emailDomains 与已知域名并去重
        var domains = new List<string>();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var d in ParseQuotedJsonArray(html, "emailDomains"))
            if (seen.Add(d)) domains.Add(d);
        foreach (var d in KnownDomains)
            if (seen.Add(d)) domains.Add(d);
        if (domains.Count == 0) domains.AddRange(KnownDomains);

        var blocked = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var u in ParseQuotedJsonArray(html, "blockedUsernames")) blocked.Add(u);

        string? domHint = (domain is not null && domain.Trim().Contains('.')) ? domain.Trim() : null;
        var mailDomain = PickMailboxDomain(domHint, domains);

        var local = "";
        for (var i = 0; i < 32; i++)
        {
            var cand = RandomLocal(10);
            if (!blocked.Contains(cand)) { local = cand; break; }
        }
        if (local.Length == 0) throw new Exception("10minute-one: could not pick username");

        var address = $"{local}@{mailDomain}";
        return new EmailInfo(selectedChannel, address, token, JwtExpMs(token));
    }

    /// <summary>获取 10minute-one 邮件列表（token 为 JWT）</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var tok = token ?? "";
        var url = $"{ApiBase}/mailbox/{EncMailboxEmail(email)}";
        var r = Http.Get(url, ApiHeaders(tok));
        r.EnsureSuccess();
        var data = Json.Parse(r.Body) as JsonArray
            ?? throw new Exception("10minute-one: invalid inbox JSON");

        var result = new List<Email>();
        foreach (var raw in data)
        {
            if (raw is not JsonObject ro) continue;
            var row = Json.ToDict(ro);
            if (ItemNeedsDetail(ro))
            {
                var mid = Json.Str(ro, "id");
                var du = $"{ApiBase}/mailbox/{EncMailboxEmail(email)}/{WebUtility.UrlEncode(mid)}";
                try
                {
                    var d = Http.Get(du, ApiHeaders(tok));
                    if (d.Ok && Json.Parse(d.Body) is JsonObject det)
                        foreach (var kv in det)
                            if (!row.ContainsKey(kv.Key)) row[kv.Key] = Json.ToRaw(kv.Value);
                }
                catch { /* 单封详情失败忽略 */ }
            }
            result.Add(Normalize.NormalizeEmail(row, email));
        }
        return result;
    }
}
