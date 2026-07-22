using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// GuerrillaMail 及其全部镜像渠道实现。
/// 所有镜像共用同一套 ajax.php API，仅 baseUrl 不同：
/// guerrillamail.com / sharklasers.com / grr.la / guerrillamail.info / spam4.me 等。
/// </summary>
public static class GuerrillaMail
{
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    };

    /// <summary>创建临时邮箱（指定镜像 baseUrl 与渠道标识）</summary>
    public static EmailInfo Generate(string baseUrl, string channel)
    {
        var resp = Http.Get($"{baseUrl}?f=get_email_address&lang=en", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        var email = Json.Str(data, "email_addr");
        var token = Json.Str(data, "sid_token");
        if (string.IsNullOrEmpty(email) || string.IsNullOrEmpty(token))
            throw new Exception($"{channel}: missing email_addr or sid_token");

        long? expiresAt = null;
        if (data is JsonObject o && o.TryGetPropertyValue("email_timestamp", out var ts)
            && ts is not null && long.TryParse(ts.ToString(), out var t))
            expiresAt = (t + 3600) * 1000;

        return new EmailInfo(channel, email, token, expiresAt);
    }

    /// <summary>获取邮件列表，对每封调用 fetch_email 拉正文</summary>
    public static List<Email> GetEmails(string baseUrl, string? token, string email)
    {
        var tok = token ?? "";
        var resp = Http.Get($"{baseUrl}?f=check_email&seq=0&sid_token={WebUtility.UrlEncode(tok)}", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);

        var list = (data as JsonObject)?["list"] as JsonArray;
        var result = new List<Email>();
        if (list is null) return result;

        foreach (var item in list)
        {
            if (item is not JsonObject msg) continue;
            var body = Json.Str(msg, "mail_body");
            var mailId = Json.Str(msg, "mail_id");
            if (string.IsNullOrEmpty(body) && !string.IsNullOrEmpty(mailId))
            {
                try
                {
                    var dr = Http.Get($"{baseUrl}?f=fetch_email&sid_token={WebUtility.UrlEncode(tok)}&email_id={WebUtility.UrlEncode(mailId)}", Headers);
                    if (dr.Ok) body = Json.Str(Json.Parse(dr.Body), "mail_body");
                }
                catch { /* 忽略单封失败 */ }
            }
            var text = string.IsNullOrEmpty(body) ? Json.Str(msg, "mail_excerpt") : Normalize.HtmlToText(body);
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = mailId,
                ["from"] = Json.Str(msg, "mail_from"),
                ["to"] = email,
                ["subject"] = Json.Str(msg, "mail_subject"),
                ["text"] = text,
                ["html"] = body,
                ["date"] = Json.Str(msg, "mail_date"),
                ["isRead"] = Json.Str(msg, "mail_read") == "1",
            }, email));
        }
        return result;
    }
}
