using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// tempemail.co 渠道 — https://tempemail.co（Cookie Session + REST JSON）。
/// 创建：GET /mail/random 获取随机邮箱地址与 session cookie，token 序列化 {"address","cookies"}。
/// 收信：GET /get-mails?mail_id=..&unseen=0&is_new=1 取列表 HTML，正则提取 data-id，
///       逐封 GET /mail/info?id={id} 取详情。
/// </summary>
public static class TempEmailCo
{
    private const string BaseUrl = "https://tempemail.co";
    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";
    private static readonly Regex MailIdRe = new("data-id=\"(\\d+)\"", RegexOptions.Compiled);

    private static Dictionary<string, string> BuildHeaders(string referer, string cookie, string accept)
    {
        var h = new Dictionary<string, string>
        {
            ["Accept-Language"] = "en-US,en;q=0.9",
            ["Cache-Control"] = "no-cache",
            ["DNT"] = "1",
            ["Pragma"] = "no-cache",
            ["Upgrade-Insecure-Requests"] = "1",
            ["User-Agent"] = Ua,
            ["Accept"] = string.IsNullOrEmpty(accept)
                ? "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8"
                : accept,
        };
        if (!string.IsNullOrEmpty(referer)) h["Referer"] = referer;
        if (!string.IsNullOrEmpty(cookie)) h["Cookie"] = cookie;
        return h;
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var r = Http.Get($"{BaseUrl}/mail/random",
            BuildHeaders($"{BaseUrl}/", "", "application/json, text/javascript, */*; q=0.01"));
        r.EnsureSuccess();
        var data = Json.Parse(r.Body) as JsonObject;
        if (!((data?["result"] as JsonValue)?.GetValue<bool>() ?? false))
            throw new Exception("tempemail-co: 创建邮箱失败");
        var address = Json.Str(data, "address");
        if (string.IsNullOrEmpty(address)) address = Json.Str(data, "id");
        address = address.Trim();
        if (string.IsNullOrEmpty(address) || !address.Contains('@'))
            throw new Exception("tempemail-co: 返回的邮箱地址无效");

        var cookies = ProviderHttpUtil.CookieHeaderFrom(r);
        var token = Json.Serialize(new Dictionary<string, object?>
        {
            ["address"] = address,
            ["cookies"] = ProviderHttpUtil.ParseCookieMap(cookies),
        });
        return new EmailInfo("tempemail-co", address, token);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("tempemail-co: token 不能为空");
        var addr = (email ?? "").Trim();
        if (string.IsNullOrEmpty(addr)) throw new Exception("tempemail-co: 邮箱地址不能为空");

        var parsed = Json.Parse(token) as JsonObject ?? throw new Exception("tempemail-co: token 格式无效");
        var address = Json.Str(parsed, "address");
        var cookieStr = CookiesToStr(parsed["cookies"] as JsonObject);
        if (string.IsNullOrEmpty(address)) throw new Exception("tempemail-co: token 数据不完整");

        var listUrl = $"{BaseUrl}/get-mails?mail_id={WebUtility.UrlEncode(address)}&unseen=0&is_new=1";
        var r = Http.Get(listUrl, BuildHeaders($"{BaseUrl}/", cookieStr, "application/json, text/javascript, */*; q=0.01"));
        r.EnsureSuccess();
        var data = Json.Parse(r.Body) as JsonObject;
        var count = (data?["count"] as JsonValue)?.TryGetValue<double>(out var c) ?? false ? c : 0;
        if (count <= 0) return new List<Email>();
        var mailsHtml = Json.Str(data, "mails");
        if (string.IsNullOrEmpty(mailsHtml)) return new List<Email>();

        var seen = new HashSet<string>();
        var uniqueIds = new List<string>();
        foreach (Match m in MailIdRe.Matches(mailsHtml))
        {
            var id = m.Groups[1].Value;
            if (seen.Add(id)) uniqueIds.Add(id);
        }

        var result = new List<Email>();
        foreach (var mailId in uniqueIds)
        {
            try
            {
                var dr = Http.Get($"{BaseUrl}/mail/info?id={WebUtility.UrlEncode(mailId)}",
                    BuildHeaders($"{BaseUrl}/", cookieStr, "application/json, text/javascript, */*; q=0.01"));
                if (dr.StatusCode != 200) continue;
                var detail = Json.Parse(dr.Body) as JsonObject;
                if (!((detail?["result"] as JsonValue)?.GetValue<bool>() ?? false)) continue;
                if (detail?["mail"] is not JsonObject info) continue;

                var fromName = Json.Str(info, "fromName").Trim();
                var fromAddr = Json.Str(info, "fromAddress").Trim();
                var fromDisplay = string.IsNullOrEmpty(fromName) ? fromAddr : $"{fromName} <{fromAddr}>";

                result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
                {
                    ["id"] = mailId,
                    ["from"] = fromDisplay,
                    ["from_address"] = fromAddr,
                    ["to"] = addr,
                    ["subject"] = Json.Str(info, "subject"),
                    ["html"] = Json.Str(info, "textHtml"),
                    ["date"] = Json.Str(info, "displayDate"),
                }, addr));
            }
            catch { /* 单封失败忽略 */ }
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
