using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>Inboxes 渠道（inboxes.com/api/v2）。GET JSON API，逐封拉取详情。</summary>
public static class Inboxes
{
    private const string ApiBase = "https://inboxes.com/api/v2";
    private const string DefaultDomain = "blondmail.com";
    private static readonly Random Rand = new();

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["Origin"] = "https://inboxes.com",
        ["Referer"] = "https://inboxes.com/",
        ["User-Agent"] = "Mozilla/5.0",
    };

    private static string RandomLocal()
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder("sdk");
        for (var i = 0; i < 16; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static List<string> GetDomains()
    {
        var resp = Http.Get($"{ApiBase}/domain", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var items = data?["domains"] as JsonArray;
        var domains = new List<string>();
        if (items is not null)
            foreach (var item in items)
                if (item is JsonObject o)
                {
                    var qdn = (o["qdn"]?.GetValue<string>() ?? "").Trim();
                    if (qdn.Length > 0) domains.Add(qdn);
                }
        if (domains.Count == 0) throw new Exception("inboxes: no domains");
        return domains;
    }

    private static string PickDomain(List<string> domains, string? preferred)
    {
        var wanted = (preferred ?? "").Trim().TrimStart('@').ToLowerInvariant();
        if (wanted.Length > 0)
            foreach (var d in domains)
                if (d.ToLowerInvariant() == wanted) return d;
        if (domains.Contains(DefaultDomain)) return DefaultDomain;
        return domains[0];
    }

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient)
    {
        return new Dictionary<string, object?>
        {
            ["id"] = raw["uid"]?.GetValue<string>() ?? "",
            ["from"] = Pick(raw, "sf", "f"),
            ["to"] = PickOr(raw, recipient, "ib"),
            ["subject"] = Pick(raw, "s"),
            ["text"] = Pick(raw, "text"),
            ["html"] = Pick(raw, "html"),
            ["preview_text"] = Pick(raw, "ph"),
            ["date"] = Pick(raw, "cr"),
            ["isRead"] = !string.IsNullOrEmpty(Pick(raw, "ru")),
            ["attachments"] = Json.ToRaw(raw["at"] as JsonArray),
        };
    }

    /// <summary>创建临时邮箱（可指定域名）</summary>
    public static EmailInfo Generate(string? domain = null)
    {
        var domains = GetDomains();
        var selected = PickDomain(domains, domain);
        return new EmailInfo("inboxes", $"{RandomLocal()}@{selected}");
    }

    private static JsonObject? FetchDetail(string uid)
    {
        try
        {
            var resp = Http.Get($"{ApiBase}/message/{WebUtility.UrlEncode(uid)}", Headers);
            if (!resp.Ok) return null;
            return Json.Parse(resp.Body) as JsonObject;
        }
        catch { return null; }
    }

    /// <summary>获取邮件列表，逐封补全详情</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("inboxes: empty email");

        var resp = Http.Get($"{ApiBase}/inbox/{WebUtility.UrlEncode(address)}", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data?["msgs"] is not JsonArray rows) return result;

        foreach (var r in rows)
        {
            if (r is not JsonObject row) continue;
            var uid = (row["uid"]?.GetValue<string>() ?? "").Trim();
            var detail = uid.Length > 0 ? FetchDetail(uid) : null;
            result.Add(Normalize.NormalizeEmail(Flatten(detail ?? row, address), address));
        }
        return result;
    }

    private static string Pick(JsonObject o, params string[] keys)
    {
        foreach (var k in keys)
        {
            if (o[k] is JsonNode n)
            {
                var s = Json.NodeToString(n);
                if (!string.IsNullOrEmpty(s)) return s;
            }
        }
        return "";
    }

    private static string PickOr(JsonObject o, string fallback, params string[] keys)
    {
        var v = Pick(o, keys);
        return v.Length > 0 ? v : fallback;
    }
}
