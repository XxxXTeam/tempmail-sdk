using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// PlainlyKit — https://plainlykit.com
/// 创建邮箱返回内部令牌；内部令牌格式为 “邮箱ID|读取令牌”。
/// </summary>
public static class PlainlyKit
{
    private const string ApiBase = "https://plainlykit.com/tools/temporary-email/api";

    private static Dictionary<string, string> BearerHeaders(string bearer) => new()
    {
        ["Accept"] = "application/json",
        ["Authorization"] = $"Bearer {bearer}",
    };

    /// <summary>创建 PlainlyKit 临时邮箱</summary>
    public static EmailInfo Generate(string channel)
    {
        var headers = new Dictionary<string, string>
        {
            ["Accept"] = "application/json",
            ["Content-Type"] = "application/json",
        };
        var resp = Http.Post($"{ApiBase}/mailboxes", "{}", "application/json", headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var address = data is null ? "" : Json.Str(data, "address").Trim();
        var id = data is null ? "" : Json.Str(data, "id").Trim();
        var token = data is null ? "" : Json.Str(data, "token").Trim();
        if (address.Length == 0 || id.Length == 0 || token.Length == 0)
            throw new Exception("plainlykit: incomplete creation response");
        return new EmailInfo(channel, address, $"{id}|{token}", createdAt: data?["createdAt"]?.ToString());
    }

    /// <summary>获取 PlainlyKit 邮件，详情提供纯文本正文</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token) || !token.Contains('|'))
            throw new Exception("plainlykit: malformed internal token");
        var parts = token.Split('|', 2);
        var basePath = $"{ApiBase}/mailboxes/{Uri.EscapeDataString(parts[0])}/messages";

        var resp = Http.Get(basePath, BearerHeaders(parts[1]));
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
                $"{basePath}/{Uri.EscapeDataString(messageId)}", BearerHeaders(parts[1]));
            if (detailResp.Ok && Json.Parse(detailResp.Body) is JsonObject detail)
            {
                var raw = detail["message"] as JsonObject ?? detail;
                var from = Json.Str(raw, "sender");
                if (from.Length == 0) from = Json.Str(io, "sender");
                var subject = Json.Str(raw, "subject");
                if (subject.Length == 0) subject = Json.Str(io, "subject");
                result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
                {
                    ["id"] = string.IsNullOrEmpty(Json.Str(raw, "id")) ? messageId : Json.Str(raw, "id"),
                    ["from"] = from,
                    ["to"] = email,
                    ["subject"] = subject,
                    ["text"] = Json.Str(raw, "text"),
                    ["html"] = Json.Str(raw, "html"),
                    ["date"] = Json.Str(raw, "receivedAt"),
                }, email));
                continue;
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = messageId,
                ["from"] = Json.Str(io, "sender"),
                ["to"] = email,
                ["subject"] = Json.Str(io, "subject"),
                ["text"] = Json.Str(io, "preview"),
                ["date"] = Json.Str(io, "receivedAt"),
            }, email));
        }
        return result;
    }
}
