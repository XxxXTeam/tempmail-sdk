using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Tsukaisute Mail — https://tsukaisutemail.com
/// 无需密钥；创建随机地址后，地址即读取凭据。
/// </summary>
public static class TsukaisuteMail
{
    private const string ApiBase = "https://tsukaisutemail.com/api/v1";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
    };

    /// <summary>创建 Tsukaisute Mail 临时地址</summary>
    public static EmailInfo Generate(string channel)
    {
        var resp = Http.Post($"{ApiBase}/address", null, null, Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var address = data is null ? "" : Json.Str(data, "address").Trim();
        if (address.Length == 0) throw new Exception("tsukaisutemail: empty address");
        return new EmailInfo(channel, address);
    }

    /// <summary>获取 Tsukaisute Mail 邮件，逐条补全正文</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("tsukaisutemail: empty email");
        var pathAddress = Uri.EscapeDataString(address).Replace("%40", "@");

        var resp = Http.Get($"{ApiBase}/inbox/{pathAddress}", Headers);
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

            var detailResp = Http.Get($"{ApiBase}/message/{Uri.EscapeDataString(messageId)}", Headers);
            if (detailResp.Ok && (Json.Parse(detailResp.Body) as JsonObject)?["message"] is JsonObject message)
            {
                result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
                {
                    ["id"] = Json.Str(message, "id"),
                    ["from"] = Json.Str(message, "from_address"),
                    ["to"] = address,
                    ["subject"] = Json.Str(message, "subject"),
                    ["text"] = Json.Str(message, "text_body"),
                    ["html"] = Json.Str(message, "html_body"),
                    ["date"] = Json.Str(message, "received_at"),
                }, address));
                continue;
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = messageId,
                ["from"] = Json.Str(io, "from_address"),
                ["to"] = address,
                ["subject"] = Json.Str(io, "subject"),
                ["text"] = Json.Str(io, "snippet"),
                ["date"] = Json.Str(io, "received_at"),
            }, address));
        }
        return result;
    }
}
