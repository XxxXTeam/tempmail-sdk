using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Tempmailify — https://tempmailify.com
/// 创建邮箱返回收件箱专属 Bearer 令牌；详情提供完整 text/html。
/// </summary>
public static class Tempmailify
{
    private const string ApiBase = "https://tempmailify.com";

    private static Dictionary<string, string> BearerHeaders(string bearer) => new()
    {
        ["Accept"] = "application/json",
        ["Authorization"] = $"Bearer {bearer}",
    };

    private static string FromAddress(JsonObject raw)
    {
        var value = raw["from"];
        if (value is JsonObject obj) return Json.Str(obj, "address");
        return value?.GetValue<string>() ?? "";
    }

    /// <summary>创建 Tempmailify 临时邮箱</summary>
    public static EmailInfo Generate(string channel)
    {
        var headers = new Dictionary<string, string>
        {
            ["Accept"] = "application/json",
            ["Content-Type"] = "application/json",
        };
        var resp = Http.Post($"{ApiBase}/api/inbox", "{}", "application/json", headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var address = data is null ? "" : Json.Str(data, "address").Trim();
        var token = data is null ? "" : Json.Str(data, "token").Trim();
        if (address.Length == 0 || token.Length == 0)
            throw new Exception("tempmailify: incomplete creation response");
        return new EmailInfo(channel, address, token, createdAt: data?["createdAt"]?.ToString());
    }

    /// <summary>获取 Tempmailify 邮件，详情提供完整 text/html</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("tempmailify: empty token");
        var address = (email ?? "").Trim();
        var basePath = $"{ApiBase}/api/inbox/{Uri.EscapeDataString(address)}";

        var resp = Http.Get(basePath, BearerHeaders(token));
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
                $"{basePath}/messages/{Uri.EscapeDataString(messageId)}", BearerHeaders(token));
            if (detailResp.Ok && Json.Parse(detailResp.Body) is JsonObject detail)
            {
                var flat = new Dictionary<string, object?>
                {
                    ["id"] = string.IsNullOrEmpty(Json.Str(detail, "id")) ? messageId : Json.Str(detail, "id"),
                    ["from"] = FromAddress(detail),
                    ["to"] = string.IsNullOrEmpty(Json.Str(detail, "to")) ? address : Json.Str(detail, "to"),
                    ["subject"] = string.IsNullOrEmpty(Json.Str(detail, "subject"))
                        ? Json.Str(io, "subject") : Json.Str(detail, "subject"),
                    ["text"] = Json.Str(detail, "text"),
                    ["html"] = Json.Str(detail, "html"),
                    ["date"] = string.IsNullOrEmpty(Json.Str(detail, "receivedAt"))
                        ? Json.Str(io, "receivedAt") : Json.Str(detail, "receivedAt"),
                    ["seen"] = Json.Str(detail, "seen"),
                    ["attachments"] = Json.ToRaw(detail["attachments"]),
                };
                result.Add(Normalize.NormalizeEmail(flat, address));
                continue;
            }

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = messageId,
                ["from"] = FromAddress(io),
                ["to"] = address,
                ["subject"] = Json.Str(io, "subject"),
                ["text"] = Json.Str(io, "preview"),
                ["date"] = Json.Str(io, "receivedAt"),
                ["seen"] = Json.Str(io, "seen"),
            }, address));
        }
        return result;
    }
}
