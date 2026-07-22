using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// altmails 渠道 — https://tempmail.altmails.com
/// Cookie Session + CSRF + REST JSON API。
/// 创建：GET / 取 session/CSRF → GET /random-email-address 取随机邮箱。
/// 收信：POST /fetch-emails/{email} 取列表 → GET /view/{id} 取正文。
/// token 存 JSON {"csrf","cookie"}。
/// </summary>
public static class Altmails
{
    private const string BaseUrl = "https://tempmail.altmails.com";

    // 从首页 inline script 中提取 CSRF token: '_token': 'xxx'
    private static readonly Regex CsrfRe = new(@"['""]_token['""]\s*:\s*['""]([^'""]+)['""]", RegexOptions.Compiled);

    private static readonly string Ua =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static Dictionary<string, string> BuildHeaders(Dictionary<string, string>? extra = null)
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
        if (extra is not null)
            foreach (var kv in extra) h[kv.Key] = kv.Value;
        return h;
    }

    /// <summary>创建 altmails 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var r = Http.Get(BaseUrl + "/", BuildHeaders());
        r.EnsureSuccess();

        var m = CsrfRe.Match(r.Body);
        if (!m.Success) throw new Exception("altmails: 无法从首页提取 CSRF token");
        var csrf = m.Groups[1].Value;

        var cookie = ProviderHttpUtil.CookieHeaderFrom(r);

        var r2 = Http.Get(BaseUrl + "/random-email-address",
            BuildHeaders(new() { ["Cookie"] = cookie, ["Referer"] = BaseUrl + "/" }));
        r2.EnsureSuccess();

        var mailbox = (r2.Body ?? "").Trim();
        if (string.IsNullOrEmpty(mailbox) || !mailbox.Contains('@'))
            throw new Exception($"altmails: 返回的邮箱地址无效: {mailbox}");

        var merged = ProviderHttpUtil.MergeCookies(cookie, r2);
        var token = Json.Serialize(new Dictionary<string, object?> { ["csrf"] = csrf, ["cookie"] = merged });
        return new EmailInfo("altmails", mailbox, token);
    }

    /// <summary>获取 altmails 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("altmails: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("altmails: 邮箱地址不能为空");

        var obj = Json.Parse(token) as JsonObject ?? throw new Exception("altmails: token 格式无效");
        var csrf = Json.Str(obj, "csrf");
        var cookie = Json.Str(obj, "cookie");

        var r = Http.Post($"{BaseUrl}/fetch-emails/{addr}",
            $"_token={WebUtility.UrlEncode(csrf)}",
            "application/x-www-form-urlencoded",
            BuildHeaders(new()
            {
                ["Accept"] = "application/json",
                ["X-Requested-With"] = "XMLHttpRequest",
                ["Cookie"] = cookie,
                ["Referer"] = BaseUrl + "/",
            }));
        r.EnsureSuccess();

        var messages = Json.Parse(r.Body) as JsonArray;
        var result = new List<Email>();
        if (messages is null) return result;

        foreach (var node in messages)
        {
            if (node is not JsonObject msg) continue;
            var id = Json.Str(msg, "id");

            var html = "";
            if (!string.IsNullOrEmpty(id))
            {
                try
                {
                    var view = Http.Get($"{BaseUrl}/view/{id}",
                        BuildHeaders(new() { ["Cookie"] = cookie, ["Referer"] = BaseUrl + "/" }));
                    if (view.StatusCode == 200) html = view.Body;
                }
                catch { /* 单封失败不影响其他 */ }
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = id,
                ["from"] = Json.Str(msg, "from"),
                ["subject"] = Json.Str(msg, "subject"),
                ["html"] = html,
                ["to"] = addr,
                ["date"] = Json.Str(msg, "date"),
            }, addr));
        }
        return result;
    }
}
