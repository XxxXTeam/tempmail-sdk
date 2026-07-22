using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// tempemails.net 渠道 — https://tempemails.net（Laravel Cookie Session + CSRF）。
/// 创建：GET / 提取 CSRF token 与 session cookie → POST /get_messages 获取分配的邮箱地址。
///       token 序列化保存 {"csrf","cookies"}。
/// 收信：POST /get_messages 取 messages 数组，逐封 GET /view/{id} 取 HTML 正文。
/// </summary>
public static class TempEmailsNet
{
    private const string BaseUrl = "https://tempemails.net";
    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";
    private static readonly Regex CsrfRe = new("<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"", RegexOptions.Compiled);

    private static Dictionary<string, string> BuildHeaders(IDictionary<string, string>? extra = null)
    {
        var h = new Dictionary<string, string>
        {
            ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
            ["Accept-Language"] = "en-US,en;q=0.9",
            ["Cache-Control"] = "no-cache",
            ["DNT"] = "1",
            ["Pragma"] = "no-cache",
            ["Upgrade-Insecure-Requests"] = "1",
            ["User-Agent"] = Ua,
        };
        if (extra is not null) foreach (var kv in extra) h[kv.Key] = kv.Value;
        return h;
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var r = Http.Get($"{BaseUrl}/", BuildHeaders());
        r.EnsureSuccess();
        var m = CsrfRe.Match(r.Body);
        if (!m.Success) throw new Exception("tempemails-net: 无法从首页提取 CSRF token");
        var csrf = m.Groups[1].Value;
        var cookieStr = ProviderHttpUtil.CookieHeaderFrom(r);

        var r2 = Http.Post($"{BaseUrl}/get_messages", null, null, BuildHeaders(new Dictionary<string, string>
        {
            ["Accept"] = "application/json",
            ["X-Requested-With"] = "XMLHttpRequest",
            ["X-CSRF-TOKEN"] = csrf,
            ["Cookie"] = cookieStr,
            ["Referer"] = $"{BaseUrl}/",
        }));
        r2.EnsureSuccess();
        var data = Json.Parse(r2.Body) as JsonObject;
        if (!((data?["status"] as JsonValue)?.GetValue<bool>() ?? false))
            throw new Exception("tempemails-net: 获取邮箱失败");
        var mailbox = Json.Str(data, "mailbox").Trim();
        if (string.IsNullOrEmpty(mailbox) || !mailbox.Contains('@'))
            throw new Exception("tempemails-net: 返回的邮箱地址无效");

        var mergedCookie = ProviderHttpUtil.MergeCookies(cookieStr, r2);
        var token = Json.Serialize(new Dictionary<string, object?>
        {
            ["csrf"] = csrf,
            ["cookies"] = ProviderHttpUtil.ParseCookieMap(mergedCookie),
        });
        return new EmailInfo("tempemails-net", mailbox, token);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("tempemails-net: token 不能为空");
        var addr = (email ?? "").Trim();
        if (string.IsNullOrEmpty(addr)) throw new Exception("tempemails-net: 邮箱地址不能为空");

        var parsed = Json.Parse(token) as JsonObject ?? throw new Exception("tempemails-net: token 格式无效");
        var csrf = Json.Str(parsed, "csrf");
        var cookieStr = CookiesToStr(parsed["cookies"] as JsonObject);
        if (string.IsNullOrEmpty(csrf)) throw new Exception("tempemails-net: token 数据不完整");

        var r = Http.Post($"{BaseUrl}/get_messages", null, null, BuildHeaders(new Dictionary<string, string>
        {
            ["Accept"] = "application/json",
            ["X-Requested-With"] = "XMLHttpRequest",
            ["X-CSRF-TOKEN"] = csrf,
            ["Cookie"] = cookieStr,
            ["Referer"] = $"{BaseUrl}/",
        }));
        r.EnsureSuccess();
        var messages = (Json.Parse(r.Body) as JsonObject)?["messages"] as JsonArray;
        var result = new List<Email>();
        if (messages is null) return result;

        foreach (var mm in messages)
        {
            if (mm is not JsonObject msg) continue;
            var mailId = Json.Str(msg, "id");
            var htmlBody = "";
            if (!string.IsNullOrEmpty(mailId))
            {
                try
                {
                    var vr = Http.Get($"{BaseUrl}/view/{mailId}", BuildHeaders(new Dictionary<string, string>
                    {
                        ["Cookie"] = cookieStr,
                        ["Referer"] = $"{BaseUrl}/",
                    }));
                    if (vr.StatusCode == 200) htmlBody = vr.Body;
                }
                catch { /* 单封失败忽略 */ }
            }

            var fromName = Json.Str(msg, "from").Trim();
            var fromAddr = Json.Str(msg, "from_email").Trim();
            var fromDisplay = !string.IsNullOrEmpty(fromName) && !string.IsNullOrEmpty(fromAddr)
                ? $"{fromName} <{fromAddr}>"
                : (!string.IsNullOrEmpty(fromAddr) ? fromAddr : fromName);

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = mailId,
                ["from"] = fromDisplay,
                ["from_address"] = fromAddr,
                ["to"] = addr,
                ["subject"] = Json.Str(msg, "subject"),
                ["html"] = htmlBody,
                ["date"] = Json.Str(msg, "receivedAt"),
            }, addr));
        }
        return result;
    }

    private static string CookiesToStr(JsonObject? cookies)
    {
        if (cookies is null) return "";
        var map = new SortedDictionary<string, string>(StringComparer.Ordinal);
        foreach (var kv in cookies) map[kv.Key] = Json.NodeToString(kv.Value);
        var parts = new List<string>();
        foreach (var kv in map) parts.Add($"{kv.Key}={kv.Value}");
        return string.Join("; ", parts);
    }
}
