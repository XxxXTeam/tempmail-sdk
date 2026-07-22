using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// MailForSpam 渠道家族 — https://mailforspam.com
/// 匿名 REST API；默认域名 mailforspam.com，tempmail.io / disposable.email 为别名。
/// </summary>
public static class MailForSpam
{
    private const string ApiBase = "https://api.mailforspam.com/api";
    private const string DefaultDomain = "mailforspam.com";
    private static readonly string[] Domains = { "mailforspam.com", "tempmail.io", "disposable.email" };
    private static readonly Random Rand = new();

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["Referer"] = "https://mailforspam.com/",
        ["Origin"] = "https://mailforspam.com",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static string RandomLocal()
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder("sdk");
        for (var i = 0; i < 16; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string PickDomain(string? preferred)
    {
        var p = (preferred ?? "").Trim().TrimStart('@').ToLowerInvariant();
        if (p.Length > 0)
            foreach (var d in Domains)
                if (d.ToLowerInvariant() == p) return d;
        return DefaultDomain;
    }

    private static string MailboxEmailsUrl(string email, int pageSize)
        => $"{ApiBase}/mailboxes/{WebUtility.UrlEncode(email)}/emails?page=1&page_size={pageSize}&sort_by=date&sort_order=desc";

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient)
    {
        var receiver = Json.Str(raw, "receiver");
        // readAt 非 null 视为已读
        var readAt = raw["readAt"];
        var isRead = readAt is not null && readAt.GetValueKind() != System.Text.Json.JsonValueKind.Null;
        return new Dictionary<string, object?>
        {
            ["id"] = Json.Str(raw, "id"),
            ["from"] = Json.Str(raw, "sender"),
            ["to"] = string.IsNullOrEmpty(receiver) ? recipient : receiver,
            ["subject"] = Json.Str(raw, "subject"),
            ["text"] = Json.Str(raw, "body_text"),
            ["html"] = Json.Str(raw, "body_html"),
            ["date"] = Json.Str(raw, "date"),
            ["isRead"] = isRead,
            ["attachments"] = Json.ToRaw(raw["attachments"]),
        };
    }

    private static JsonObject? FetchMessage(string messageId, string email)
    {
        var resp = Http.Get($"{ApiBase}/mailboxes/{WebUtility.UrlEncode(email)}/emails/{WebUtility.UrlEncode(messageId)}", Headers);
        if (!resp.Ok) return null;
        return Json.Parse(resp.Body) as JsonObject;
    }

    /// <summary>创建 mailforspam 临时邮箱（指定渠道标识与域名）</summary>
    public static EmailInfo Generate(string channel, string? domain)
    {
        var email = $"{RandomLocal()}@{PickDomain(domain)}";
        var resp = Http.Get(MailboxEmailsUrl(email, 1), Headers);
        resp.EnsureSuccess();
        return new EmailInfo(channel, email);
    }

    /// <summary>获取 mailforspam 邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("mailforspam: empty email");

        var resp = Http.Get(MailboxEmailsUrl(address, 100), Headers);
        resp.EnsureSuccess();
        var rows = (Json.Parse(resp.Body) as JsonObject)?["emails"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var item in rows)
        {
            if (item is not JsonObject io) continue;
            var messageId = Json.Str(io, "id").Trim();
            if (messageId.Length == 0) continue;
            var detail = FetchMessage(messageId, address);
            result.Add(Normalize.NormalizeEmail(Flatten(detail ?? io, address), address));
        }
        return result;
    }
}
