using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// tempmails.cc — https://tempmails.cc
/// 地址即邮箱：在公开免费域名上生成地址，无需注册。
/// 详情正文位于 textBody / htmlBody。
/// </summary>
public static class TempmailsCc
{
    private const string ApiBase = "https://tempmails.cc";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
    };

    private static string PickDomain()
    {
        var resp = Http.Get($"{ApiBase}/api/domains", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data?["domainInfo"] is JsonArray items)
        {
            foreach (var item in items)
            {
                if (item is not JsonObject io) continue;
                var domain = Json.Str(io, "domain").Trim();
                if (domain.Length > 0) return domain;
            }
        }
        throw new Exception("tempmails-cc: no public domain");
    }

    /// <summary>在公开免费域名上生成地址</summary>
    public static EmailInfo Generate(string channel)
    {
        var domain = PickDomain();
        var local = Guid.NewGuid().ToString("N").Substring(0, 16);
        return new EmailInfo(channel, $"{local}@{domain}");
    }

    /// <summary>跳过选域，在指定固定域名上生成地址</summary>
    public static EmailInfo GenerateAtDomain(string channel, string domain)
    {
        var selected = (domain ?? "").Trim();
        if (selected.Length == 0) throw new Exception("tempmails-cc: empty domain");
        var local = Guid.NewGuid().ToString("N").Substring(0, 16);
        return new EmailInfo(channel, $"{local}@{selected}");
    }

    /// <summary>获取 tempmails.cc 邮件，详情正文位于 textBody / htmlBody</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("tempmails-cc: empty email");

        var resp = Http.Get($"{ApiBase}/api/inbox/{Uri.EscapeDataString(address)}", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var rows = data?["messages"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var item in rows)
        {
            if (item is not JsonObject io) continue;
            var messageId = Json.Str(io, "id").Trim();
            if (messageId.Length == 0) continue;

            var detailResp = Http.Get(
                $"{ApiBase}/api/message/{Uri.EscapeDataString(messageId)}?address={Uri.EscapeDataString(address)}&mark_read=false",
                Headers);
            if (detailResp.Ok && Json.Parse(detailResp.Body) is JsonObject detail)
            {
                result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
                {
                    ["id"] = string.IsNullOrEmpty(Json.Str(detail, "id")) ? messageId : Json.Str(detail, "id"),
                    ["from"] = Json.Str(detail, "from"),
                    ["to"] = address,
                    ["subject"] = Json.Str(detail, "subject"),
                    ["text"] = Json.Str(detail, "textBody"),
                    ["html"] = Json.Str(detail, "htmlBody"),
                    ["date"] = Json.Str(detail, "date"),
                    ["seen"] = Json.Str(detail, "isRead"),
                }, address));
                continue;
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = messageId,
                ["from"] = Json.Str(io, "from"),
                ["to"] = address,
                ["subject"] = Json.Str(io, "subject"),
                ["date"] = Json.Str(io, "date"),
                ["seen"] = Json.Str(io, "isRead"),
            }, address));
        }
        return result;
    }
}
