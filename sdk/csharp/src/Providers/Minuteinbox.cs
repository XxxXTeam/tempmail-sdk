using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// minuteinbox.com 渠道 — https://www.minuteinbox.com
/// Cookie Session + CSRF + REST JSON API（METRONET 后端）。
/// 创建：GET / 取 PHPSESSID cookie + CSRF（正则 CSRF="xxx"）→ GET /index/index?csrf_token= 建邮箱。
/// 收信：GET /index/refresh 取列表 → POST /index/email(id=) 取正文（字段 telo）。
/// token 编码 JSON {"csrf","cookies"}（cookies 为 Cookie 头字符串）。
/// </summary>
public static class Minuteinbox
{
    private const string Origin = "https://www.minuteinbox.com";

    private static readonly Regex CsrfRe = new("CSRF\\s*=\\s*\"([^\"]+)\"", RegexOptions.Compiled);

    private static string Ua()
    {
        var cfg = Config.Get();
        if (cfg.Headers is not null && cfg.Headers.TryGetValue("User-Agent", out var ua) && !string.IsNullOrEmpty(ua))
            return ua;
        return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";
    }

    private static Dictionary<string, string> DefaultHeaders() => new()
    {
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "en-US,en;q=0.9",
        ["Cache-Control"] = "no-cache",
        ["DNT"] = "1",
        ["Pragma"] = "no-cache",
        ["Upgrade-Insecure-Requests"] = "1",
        ["User-Agent"] = Ua(),
    };

    private static Dictionary<string, string> AjaxHeaders(string cookie) => new()
    {
        ["User-Agent"] = Ua(),
        ["Accept"] = "application/json, text/javascript, */*; q=0.01",
        ["Accept-Language"] = "en-US,en;q=0.9",
        ["X-Requested-With"] = "XMLHttpRequest",
        ["Referer"] = $"{Origin}/",
        ["Cookie"] = cookie,
    };

    private static string EncodeToken(string csrf, string cookie)
        => Json.Serialize(new Dictionary<string, object?> { ["csrf"] = csrf, ["cookies"] = cookie });

    private static (string csrf, string cookie) DecodeToken(string token)
    {
        var data = Json.Parse(token) as JsonObject ?? throw new Exception("minuteinbox: token 格式无效");
        var csrf = Json.Str(data, "csrf");
        var cookie = Json.Str(data, "cookies");
        if (csrf.Length == 0) throw new Exception("minuteinbox: token 数据不完整");
        return (csrf, cookie);
    }

    /// <summary>创建 minuteinbox 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var r1 = Http.Get($"{Origin}/", DefaultHeaders());
        r1.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(r1);

        var m = CsrfRe.Match(r1.Body);
        if (!m.Success) throw new Exception("minuteinbox: 无法从首页 HTML 提取 CSRF token");
        var csrf = m.Groups[1].Value;

        var r2 = Http.Get($"{Origin}/index/index?csrf_token={WebUtility.UrlEncode(csrf)}", AjaxHeaders(cookie));
        r2.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, r2);

        var data = Json.Parse(r2.Body) as JsonObject
            ?? throw new Exception($"minuteinbox: 返回的邮箱地址无效: {r2.Body}");
        var emailAddr = Json.Str(data, "email").Trim();
        if (emailAddr.Length == 0 || !emailAddr.Contains('@'))
            throw new Exception($"minuteinbox: 返回的邮箱地址无效: {r2.Body}");

        return new EmailInfo("minuteinbox", emailAddr, EncodeToken(csrf, cookie));
    }

    /// <summary>获取 minuteinbox 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("minuteinbox: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("minuteinbox: 邮箱地址不能为空");

        var (_, cookie) = DecodeToken(token);

        var r = Http.Get($"{Origin}/index/refresh", AjaxHeaders(cookie));
        r.EnsureSuccess();

        var result = new List<Email>();
        if (Json.Parse(r.Body) is not JsonArray rows) return result;

        foreach (var node in rows)
        {
            if (node is not JsonObject item) continue;
            var mailId = Json.Str(item, "id");
            if (mailId.Length == 0 && item["id"] is null) continue;

            var bodyHtml = "";
            var detailHeaders = AjaxHeaders(cookie);
            detailHeaders["Content-Type"] = "application/x-www-form-urlencoded";
            var detailResp = Http.Post($"{Origin}/index/email", $"id={mailId}",
                "application/x-www-form-urlencoded", detailHeaders);
            if (detailResp.StatusCode == 200)
            {
                if (Json.Parse(detailResp.Body) is JsonObject detail) bodyHtml = Json.Str(detail, "telo");
                else bodyHtml = detailResp.Body.Trim();
            }

            // "new" 表示未读，其他值表示已读
            var isRead = Json.Str(item, "precteno") != "new";

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = mailId,
                ["from"] = Json.Str(item, "od"),
                ["to"] = addr,
                ["subject"] = Json.Str(item, "predmet"),
                ["html"] = bodyHtml,
                ["date"] = Json.Str(item, "kdy"),
                ["is_read"] = isRead,
            }, addr));
        }
        return result;
    }
}
