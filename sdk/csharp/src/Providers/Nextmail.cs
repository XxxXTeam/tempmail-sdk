using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Nextmail — https://nextmail.sh
/// 创建收件箱返回内部令牌；内部令牌格式为 “收件箱ID|读取令牌”。
/// 详情正文位于 message.decodedBody（HTML）。
/// </summary>
public static class Nextmail
{
    private const string ApiBase = "https://api.nextmail.sh";

    private static Dictionary<string, string> BearerHeaders(string bearer) => new()
    {
        ["Accept"] = "application/json",
        ["Authorization"] = $"Bearer {bearer}",
    };

    /// <summary>租用 Nextmail 临时收件箱</summary>
    public static EmailInfo Generate(string channel)
    {
        var headers = new Dictionary<string, string>
        {
            ["Accept"] = "application/json",
            ["Content-Type"] = "application/json",
        };
        var resp = Http.Post($"{ApiBase}/temp-mail/inboxes", "{}", "application/json", headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var address = data is null ? "" : Json.Str(data, "address").Trim();
        var id = data is null ? "" : Json.Str(data, "id").Trim();
        var token = data is null ? "" : Json.Str(data, "token").Trim();
        if (address.Length == 0 || id.Length == 0 || token.Length == 0)
            throw new Exception("nextmail: incomplete creation response");
        return new EmailInfo(channel, address, $"{id}|{token}", createdAt: data?["createdAt"]?.ToString());
    }

    /// <summary>获取 Nextmail 邮件，详情正文位于 message.decodedBody</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token) || !token.Contains('|'))
            throw new Exception("nextmail: malformed internal token");
        var parts = token.Split('|', 2);
        var basePath = $"{ApiBase}/temp-mail/inboxes/{Uri.EscapeDataString(parts[0])}/messages";

        var resp = Http.Get($"{basePath}?limit=50", BearerHeaders(parts[1]));
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var rows = data?["data"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var item in rows)
        {
            if (item is not JsonObject io) continue;
            var messageId = Json.Str(io, "id").Trim();
            if (messageId.Length == 0) continue;

            var detailResp = Http.Get(
                $"{basePath}/{Uri.EscapeDataString(messageId)}", BearerHeaders(parts[1]));
            if (detailResp.Ok && Json.Parse(detailResp.Body) is JsonObject detail)
            {
                var flat = new Dictionary<string, object?>
                {
                    ["id"] = string.IsNullOrEmpty(Json.Str(detail, "id")) ? messageId : Json.Str(detail, "id"),
                    ["from"] = NestedEmail(detail, "from"),
                    ["to"] = string.IsNullOrEmpty(Json.Str(detail, "to")) ? email : Json.Str(detail, "to"),
                    ["subject"] = Json.Str(detail, "subject"),
                    ["date"] = Json.Str(detail, "receivedAt"),
                    ["seen"] = Json.Str(detail, "seen"),
                };
                if (detail["message"] is JsonObject message)
                    flat["html"] = Json.Str(message, "decodedBody");
                result.Add(Normalize.NormalizeEmail(flat, email));
                continue;
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = messageId,
                ["from"] = NestedEmail(io, "from"),
                ["to"] = string.IsNullOrEmpty(Json.Str(io, "to")) ? email : Json.Str(io, "to"),
                ["subject"] = Json.Str(io, "subject"),
                ["text"] = Json.Str(io, "snippet"),
                ["date"] = Json.Str(io, "receivedAt"),
                ["seen"] = Json.Str(io, "seen"),
            }, email));
        }
        return result;
    }

    private static string NestedEmail(JsonObject raw, string key)
    {
        var value = raw[key];
        if (value is JsonObject obj) return Json.Str(obj, "email");
        return value?.GetValue<string>() ?? "";
    }
}
