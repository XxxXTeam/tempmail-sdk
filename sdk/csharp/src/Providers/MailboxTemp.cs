using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// MailboxTemp — https://mailboxtemp.com
/// 无需密钥；创建免费邮箱后，地址即读取凭据。
/// </summary>
public static class MailboxTemp
{
    private const string ApiBase = "https://mailboxtemp.com";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
    };

    /// <summary>创建 MailboxTemp 临时邮箱</summary>
    public static EmailInfo Generate(string channel)
    {
        var resp = Http.Post($"{ApiBase}/api/inbox/generate", null, null, Headers);
        var data = Json.Parse(resp.Body) as JsonObject;
        var address = data is null ? "" : Json.Str(data, "address").Trim();
        if (!resp.Ok || address.Length == 0)
            throw new Exception($"mailboxtemp: create inbox failed HTTP {resp.StatusCode}");
        return new EmailInfo(channel, address);
    }

    /// <summary>获取 MailboxTemp 邮件，逐条补全正文</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("mailboxtemp: empty email");
        var pathAddress = Uri.EscapeDataString(address);

        var data = FetchJson($"{ApiBase}/api/inbox/{pathAddress}/emails");
        var rows = data?["emails"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var item in rows)
        {
            if (item is not JsonObject io) continue;
            var messageId = Json.Str(io, "id").Trim();
            if (messageId.Length == 0) continue;

            var detail = FetchJson(
                $"{ApiBase}/api/email/{pathAddress}/{Uri.EscapeDataString(messageId)}");
            if (detail?["email"] is JsonObject message)
            {
                result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
                {
                    ["id"] = Json.Str(message, "id"),
                    ["from"] = Json.Str(message, "from_address"),
                    ["to"] = address,
                    ["subject"] = Json.Str(message, "subject"),
                    ["text"] = Json.Str(message, "body_text"),
                    ["html"] = Json.Str(message, "body_html"),
                    ["date"] = Json.Str(message, "received_at"),
                    ["read"] = Json.Str(message, "read"),
                    ["attachments"] = Json.ToRaw(detail["attachments"]),
                }, address));
                continue;
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = messageId,
                ["from"] = Json.Str(io, "from_address"),
                ["to"] = address,
                ["subject"] = Json.Str(io, "subject"),
                ["date"] = Json.Str(io, "received_at"),
                ["read"] = Json.Str(io, "read"),
            }, address));
        }
        return result;
    }

    private static JsonObject? FetchJson(string url)
    {
        var resp = Http.Get(url, Headers);
        if (!resp.Ok) return null;
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null || (data["ok"]?.GetValue<bool>() ?? true) == false) return null;
        return data;
    }
}
