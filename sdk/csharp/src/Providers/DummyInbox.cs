using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// DummyInbox — https://dummyinbox.com
/// 无需密钥；创建随机地址后，地址本身即读取凭据。
/// </summary>
public static class DummyInbox
{
    private const string ApiBase = "https://dummyinbox.com/api/v1";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
    };

    /// <summary>创建 DummyInbox 临时邮箱</summary>
    public static EmailInfo Generate(string channel)
    {
        var resp = Http.Post($"{ApiBase}/inboxes", null, null, Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var address = data is null ? "" : Json.Str(data, "address").Trim();
        if (address.Length == 0) throw new Exception("dummyinbox: empty address");
        return new EmailInfo(channel, address);
    }

    /// <summary>获取 DummyInbox 邮件，逐条补全正文</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("dummyinbox: empty email");
        var pathAddress = Uri.EscapeDataString(address).Replace("%40", "@");

        var resp = Http.Get($"{ApiBase}/inboxes/{pathAddress}/messages?limit=50", Headers);
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
                $"{ApiBase}/inboxes/{pathAddress}/messages/{Uri.EscapeDataString(messageId)}", Headers);
            if (detailResp.Ok && (Json.Parse(detailResp.Body) as JsonObject)?["message"] is JsonObject message)
            {
                var to = "";
                if (message["to"] is JsonArray toList && toList.Count > 0)
                    to = toList[0]?.GetValue<string>() ?? "";
                result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
                {
                    ["id"] = Json.Str(message, "id"),
                    ["from"] = Json.Str(message, "from"),
                    ["to"] = to.Length == 0 ? address : to,
                    ["subject"] = Json.Str(message, "subject"),
                    ["text"] = Json.Str(message, "text"),
                    ["html"] = Json.Str(message, "html"),
                    ["date"] = Json.Str(message, "receivedAt"),
                    ["attachments"] = Json.ToRaw(message["attachments"]),
                }, address));
                continue;
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = messageId,
                ["from"] = Json.Str(io, "from"),
                ["to"] = address,
                ["subject"] = Json.Str(io, "subject"),
                ["text"] = Json.Str(io, "snippet"),
                ["date"] = Json.Str(io, "receivedAt"),
            }, address));
        }
        return result;
    }
}
