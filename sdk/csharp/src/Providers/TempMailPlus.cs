using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// TempMail Plus 渠道家族 — https://tempmail.plus
/// 无需认证、无需 Cookie、无需 Token 的简单 REST API；默认域名 mailto.plus，
/// fexpost.com / fexbox.org / mailbox.in.ua 等别名仅换域名与渠道标识。
/// </summary>
public static class TempMailPlus
{
    private const string ApiBase = "https://tempmail.plus/api/mails";
    private const string DefaultDomain = "mailto.plus";
    private const string DefaultChannel = "tempmail-plus";
    private static readonly Random Rand = new();

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        ["Referer"] = "https://tempmail.plus/",
        ["Origin"] = "https://tempmail.plus",
    };

    private static string RandomLocal(int length = 12)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>创建 tempmail-plus 临时邮箱（指定域名与渠道标识）</summary>
    public static EmailInfo Generate(string? domain = null, string? channel = null)
    {
        var selectedDomain = string.IsNullOrWhiteSpace(domain) ? DefaultDomain : domain.Trim();
        var selectedChannel = string.IsNullOrWhiteSpace(channel) ? DefaultChannel : channel.Trim();
        var email = $"{RandomLocal()}@{selectedDomain}";

        // 调用列表接口验证地址可用
        var resp = Http.Get($"{ApiBase}/?email={WebUtility.UrlEncode(email)}&epin=", Headers);
        resp.EnsureSuccess();

        return new EmailInfo(selectedChannel, email);
    }

    private static JsonObject? FetchBody(string mailId, string email)
    {
        if (string.IsNullOrEmpty(mailId)) return null;
        try
        {
            var resp = Http.Get($"{ApiBase}/{mailId}?email={WebUtility.UrlEncode(email)}&epin=", Headers);
            if (!resp.Ok) return null;
            return Json.Parse(resp.Body) as JsonObject;
        }
        catch { return null; }
    }

    /// <summary>获取 tempmail-plus 邮件列表（逐封拉取正文）</summary>
    public static List<Email> GetEmails(string email)
    {
        var e = (email ?? "").Trim();
        if (e.Length == 0) throw new Exception("tempmail-plus: 邮箱地址为空");

        var resp = Http.Get($"{ApiBase}/?email={WebUtility.UrlEncode(e)}&epin=", Headers);
        resp.EnsureSuccess();
        var rows = (Json.Parse(resp.Body) as JsonObject)?["mail_list"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var raw in rows)
        {
            if (raw is not JsonObject ro) continue;
            var mailId = Json.Str(ro, "mail_id");
            var detail = FetchBody(mailId, e);

            var from = Json.Str(ro, "from_mail");
            if (string.IsNullOrEmpty(from)) from = Json.Str(ro, "from_name");

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = mailId,
                ["from"] = from,
                ["to"] = e,
                ["subject"] = Json.Str(ro, "subject"),
                ["date"] = Json.Str(ro, "time"),
                ["html"] = detail is null ? "" : Json.Str(detail, "html"),
                ["text"] = detail is null ? "" : Json.Str(detail, "text"),
                ["isRead"] = !((ro["is_new"] as JsonValue)?.GetValue<bool>() ?? true),
            }, e));
        }
        return result;
    }
}
