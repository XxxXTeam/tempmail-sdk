using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// tmail.link 渠道 — https://tmail.link（Django CSRF）。
/// 创建：GET / 正则提取邮箱地址 → GET /inbox/{email}/ 取 Set-Cookie 中的 csrftoken，token 存该值。
/// 收信：POST /inbox/{email}/，form format=json&amp;csrfmiddlewaretoken={token}，Cookie: csrftoken={token}。
///       响应 {"messages":[{key,sender,subject,date}]}。
/// </summary>
public static class TmailLink
{
    private const string Channel = "tmail-link";
    private const string BaseUrl = "https://tmail.link";
    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";
    private static readonly Regex EmailRe = new("([a-zA-Z0-9._%+-]+@tmail\\.link)", RegexOptions.Compiled);

    private static Dictionary<string, string> BrowserHeaders() => new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
    };

    private static Dictionary<string, string> PostHeaders(string email, string token) => new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "application/json, text/javascript, */*; q=0.01",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["X-Requested-With"] = "XMLHttpRequest",
        ["Content-Type"] = "application/x-www-form-urlencoded; charset=UTF-8",
        ["Origin"] = BaseUrl,
        ["Referer"] = $"{BaseUrl}/inbox/{email}/",
        ["Cookie"] = $"csrftoken={token}",
    };

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{BaseUrl}/", BrowserHeaders());
        resp.EnsureSuccess();
        var m = EmailRe.Match(resp.Body);
        if (!m.Success) throw new Exception($"{Channel}: 未能从首页提取邮箱地址");
        var email = m.Groups[1].Value.Trim();
        if (string.IsNullOrEmpty(email)) throw new Exception($"{Channel}: 提取的邮箱地址为空");

        var resp2 = Http.Get($"{BaseUrl}/inbox/{email}/", BrowserHeaders());
        resp2.EnsureSuccess();
        var token = ProviderHttpUtil.ExtractCookieValue(resp2, "csrftoken");
        if (string.IsNullOrEmpty(token)) throw new Exception($"{Channel}: 未能获取 csrftoken");

        return new EmailInfo(Channel, email, token);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        email = (email ?? "").Trim();
        if (string.IsNullOrEmpty(email)) throw new Exception($"{Channel}: 邮箱地址为空");
        token ??= "";

        var body = $"format=json&csrfmiddlewaretoken={WebUtility.UrlEncode(token)}";
        var resp = Http.Post($"{BaseUrl}/inbox/{email}/", body,
            "application/x-www-form-urlencoded; charset=UTF-8", PostHeaders(email, token));
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject
            ?? throw new Exception($"{Channel}: 邮件列表响应格式异常");
        var result = new List<Email>();
        if (data["messages"] is not JsonArray messages) return result;

        foreach (var it in messages)
        {
            if (it is not JsonObject msg) continue;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(msg, "key"),
                ["from"] = Json.Str(msg, "sender"),
                ["to"] = email,
                ["subject"] = Json.Str(msg, "subject"),
                ["date"] = Json.Str(msg, "date"),
            }, email));
        }
        return result;
    }
}
