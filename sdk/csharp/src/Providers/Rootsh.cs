using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Rootsh(BccTo) 渠道 — https://rootsh.com
/// 需要 cookie session 认证，token 内嵌 lastCheckTime 与 cookie 用于增量取信。
/// 创建：GET / 获取 session cookie，POST /applymail 申请邮箱。
/// 取信：POST /getmail 获取列表，POST /viewmail 获取正文。
/// </summary>
public static class Rootsh
{
    private const string Base = "https://rootsh.com";
    private const string DefaultDomain = "bccto.cc";
    private const string TokPrefix = "rootsh1:";
    private static readonly Random Rand = new();

    private const string Ua =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
        "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static Dictionary<string, string> ApiHeaders(string cookie) => new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["Cache-Control"] = "no-cache",
        ["Pragma"] = "no-cache",
        ["Referer"] = $"{Base}/",
        ["X-Requested-With"] = "XMLHttpRequest",
        ["User-Agent"] = Ua,
        ["Content-Type"] = "application/x-www-form-urlencoded; charset=UTF-8",
        ["Cookie"] = cookie,
    };

    private static string RandomLocal(int length = 10)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    // token 内含 lastCheckTime 与 cookie 头串
    private static string EncodeToken(string lastCheckTime, string cookie)
        => TokPrefix + Json.Serialize(new Dictionary<string, object?> { ["t"] = lastCheckTime, ["c"] = cookie });

    private static (string LastCheckTime, string Cookie) DecodeToken(string token)
    {
        if (string.IsNullOrEmpty(token) || !token.StartsWith(TokPrefix, StringComparison.Ordinal))
            throw new Exception("rootsh: 无效的 session token");
        var node = Json.Parse(token[TokPrefix.Length..]);
        return (Json.Str(node, "t"), Json.Str(node, "c"));
    }

    public static EmailInfo Generate(string? domain)
    {
        var dom = string.IsNullOrWhiteSpace(domain) ? DefaultDomain : domain.Trim();
        var mailAddr = $"{RandomLocal()}@{dom}";

        // 第一步：GET 首页取 session cookie
        var r1 = Http.Get($"{Base}/", new Dictionary<string, string>
        {
            ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
            ["User-Agent"] = Ua,
        });
        r1.EnsureSuccess();
        var cookie = ProviderHttpUtil.CookieHeaderFrom(r1);

        // 第二步：POST /applymail 申请邮箱
        var r2 = Http.Post($"{Base}/applymail", $"mail={WebUtility.UrlEncode(mailAddr)}",
            "application/x-www-form-urlencoded; charset=UTF-8", ApiHeaders(cookie));
        r2.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, r2);

        var body = Json.Parse(r2.Body);
        if (Json.Str(body, "success") != "true")
            throw new Exception($"rootsh: 邮箱申请失败: {Json.Str(body, "tips")}");

        var actualEmail = Json.Str(body, "user").Trim();
        if (string.IsNullOrEmpty(actualEmail)) actualEmail = mailAddr;
        var lastCheckTime = Json.Str(body, "time");

        return new EmailInfo("rootsh", actualEmail, EncodeToken(lastCheckTime, cookie));
    }

    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("rootsh: token 不能为空");
        var addr = (email ?? "").Trim();
        if (string.IsNullOrEmpty(addr)) throw new Exception("rootsh: 邮箱地址不能为空");

        var (lastCheckTime, cookie) = DecodeToken(token);
        var ts = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();

        var r = Http.Post($"{Base}/getmail",
            $"mail={WebUtility.UrlEncode(addr)}&time={WebUtility.UrlEncode(lastCheckTime)}&_={ts}",
            "application/x-www-form-urlencoded; charset=UTF-8", ApiHeaders(cookie));
        r.EnsureSuccess();
        var body = Json.Parse(r.Body);

        var result = new List<Email>();
        if (Json.Str(body, "success") != "true") return result;
        if ((body as JsonObject)?["mail"] is not JsonArray mailList) return result;

        foreach (var item in mailList)
        {
            // 每个 item 是数组: [display_name, from_email, subject, date_str, mail_fid, ...]
            if (item is not JsonArray arr || arr.Count < 5) continue;
            var displayName = Json.NodeToString(arr[0]);
            var fromEmail = Json.NodeToString(arr[1]);
            var subject = Json.NodeToString(arr[2]);
            var dateStr = Json.NodeToString(arr[3]);
            var mailFid = Json.NodeToString(arr[4]);
            var fromAddr = string.IsNullOrEmpty(fromEmail) ? displayName : fromEmail;

            var html = "";
            if (!string.IsNullOrEmpty(mailFid))
            {
                try
                {
                    var rv = Http.Post($"{Base}/viewmail",
                        $"mail={WebUtility.UrlEncode(mailFid)}&to={WebUtility.UrlEncode(addr)}",
                        "application/x-www-form-urlencoded; charset=UTF-8", ApiHeaders(cookie));
                    if (rv.Ok)
                    {
                        var vb = Json.Parse(rv.Body);
                        if (Json.Str(vb, "success") == "true") html = Json.Str(vb, "mail");
                    }
                }
                catch { /* 单封失败忽略 */ }
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = mailFid,
                ["from"] = fromAddr,
                ["to"] = addr,
                ["subject"] = subject,
                ["date"] = dateStr,
                ["html"] = html,
            }, addr));
        }
        return result;
    }
}
