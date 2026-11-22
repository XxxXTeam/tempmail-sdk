using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// GrabMail — https://grabmail.io
/// 地址即邮箱：首封邮件到达时自动存在，无需创建。
/// </summary>
public static class Grabmail
{
    private const string ApiBase = "https://grabmail.io/api/v1";
    private const string Domain = "grabmail.io";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
    };

    private static string FromValue(JsonObject raw)
    {
        var name = Json.Str(raw, "from_name");
        var address = Json.Str(raw, "from");
        return name.Length == 0 ? address : $"{name} <{address}>";
    }

    /// <summary>生成 GrabMail 地址（无需请求服务端）</summary>
    public static EmailInfo Generate(string channel)
    {
        var local = "sdk" + Guid.NewGuid().ToString("N").Substring(0, 16);
        return new EmailInfo(channel, $"{local}@{Domain}");
    }

    /// <summary>获取 GrabMail 邮件，逐条补全正文</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("grabmail: empty email");

        var resp = Http.Get($"{ApiBase}/mailbox?address={Uri.EscapeDataString(address)}", Headers);
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
                $"{ApiBase}/message/{Uri.EscapeDataString(messageId)}?mailbox={Uri.EscapeDataString(address)}",
                Headers);
            if (detailResp.Ok && Json.Parse(detailResp.Body) is JsonObject detail)
            {
                result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
                {
                    ["id"] = string.IsNullOrEmpty(Json.Str(detail, "id")) ? messageId : Json.Str(detail, "id"),
                    ["from"] = FromValue(detail),
                    ["to"] = address,
                    ["subject"] = Json.Str(detail, "subject"),
                    ["text"] = Json.Str(detail, "text"),
                    ["html"] = Json.Str(detail, "html"),
                    ["date"] = Json.Str(detail, "date"),
                    ["seen"] = Json.Str(detail, "seen"),
                    ["attachments"] = Json.ToRaw(detail["attachments"]),
                }, address));
                continue;
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = messageId,
                ["from"] = FromValue(io),
                ["to"] = address,
                ["subject"] = Json.Str(io, "subject"),
                ["text"] = Json.Str(io, "preview"),
                ["date"] = Json.Str(io, "date"),
                ["seen"] = Json.Str(io, "seen"),
            }, address));
        }
        return result;
    }
}
