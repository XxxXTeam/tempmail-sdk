using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Catchmail 渠道家族 — https://catchmail.io
/// 匿名 REST API；默认域名 catchmail.io，mailistry.com / zeppost.com 为别名。
/// </summary>
public static class Catchmail
{
    private const string ApiBase = "https://api.catchmail.io/api/v1";
    private const string DefaultDomain = "catchmail.io";
    private static readonly string[] Domains = { "catchmail.io", "mailistry.com", "zeppost.com" };
    private static readonly Random Rand = new();
    private static readonly Regex AngleRe = new("<([^>]+)>", RegexOptions.Compiled);

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["Referer"] = "https://catchmail.io/",
        ["Origin"] = "https://catchmail.io",
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

    private static string CleanAddress(string value)
    {
        var text = (value ?? "").Trim();
        var m = AngleRe.Match(text);
        return m.Success ? m.Groups[1].Value.Trim() : text;
    }

    /// <summary>创建 catchmail 临时邮箱（指定渠道标识与域名）</summary>
    public static EmailInfo Generate(string channel, string? domain)
    {
        var email = $"{RandomLocal()}@{PickDomain(domain)}";
        var resp = Http.Get($"{ApiBase}/mailbox?address={WebUtility.UrlEncode(email)}", Headers);
        resp.EnsureSuccess();
        return new EmailInfo(channel, email);
    }

    private static JsonObject? FetchMessage(string messageId, string email)
    {
        var resp = Http.Get($"{ApiBase}/message/{WebUtility.UrlEncode(messageId)}?mailbox={WebUtility.UrlEncode(email)}", Headers);
        if (!resp.Ok) return null;
        return Json.Parse(resp.Body) as JsonObject;
    }

    /// <summary>获取 catchmail 邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("catchmail: empty email");

        var resp = Http.Get($"{ApiBase}/mailbox?address={WebUtility.UrlEncode(address)}", Headers);
        resp.EnsureSuccess();
        var rows = (Json.Parse(resp.Body) as JsonObject)?["messages"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var item in rows)
        {
            if (item is not JsonObject io) continue;
            var messageId = Json.Str(io, "id").Trim();
            if (messageId.Length == 0) continue;

            var detail = FetchMessage(messageId, address);
            if (detail is not null)
            {
                var body = detail["body"] as JsonObject;
                var toAddr = CleanAddress(Json.Str(detail, "to"));
                result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
                {
                    ["id"] = Json.Str(detail, "id"),
                    ["from"] = CleanAddress(Json.Str(detail, "from")),
                    ["to"] = string.IsNullOrEmpty(toAddr) ? address : toAddr,
                    ["subject"] = Json.Str(detail, "subject"),
                    ["text"] = body is null ? "" : Json.Str(body, "text"),
                    ["html"] = body is null ? "" : Json.Str(body, "html"),
                    ["date"] = Json.Str(detail, "date"),
                    ["attachments"] = Json.ToRaw(detail["attachments"]),
                }, address));
                continue;
            }

            var mailbox = Json.Str(io, "mailbox");
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = messageId,
                ["from"] = CleanAddress(Json.Str(io, "from")),
                ["to"] = string.IsNullOrEmpty(mailbox) ? address : mailbox,
                ["subject"] = Json.Str(io, "subject"),
                ["date"] = Json.Str(io, "date"),
            }, address));
        }
        return result;
    }
}
