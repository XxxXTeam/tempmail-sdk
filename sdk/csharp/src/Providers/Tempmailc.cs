using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// TempMailC 渠道（tempmailc.com/api/v1）。
/// 创建：GET /new；收信：GET /inbox?email=，逐封 GET /message 补全详情（失败回退列表项）。
/// </summary>
public static class Tempmailc
{
    private const string ApiBase = "https://tempmailc.com/api/v1";

    private static Dictionary<string, string> Headers() => new() { ["Accept"] = "application/json" };

    private static Dictionary<string, object?> FlattenListMessage(JsonObject raw, string recipient) => new()
    {
        ["id"] = Json.Str(raw, "id"),
        ["from"] = Json.Str(raw, "from"),
        ["to"] = recipient,
        ["subject"] = Json.Str(raw, "subject"),
        ["timestamp"] = raw["ts"] is JsonValue tv && tv.TryGetValue<long>(out var ts) ? ts : (object?)Json.Str(raw, "ts"),
        ["read"] = (raw["read"] as JsonValue)?.GetValue<bool>() ?? false,
    };

    private static JsonObject? FetchMessage(string email, string messageId)
    {
        var resp = Http.Get($"{ApiBase}/message?email={WebUtility.UrlEncode(email)}&msg_id={WebUtility.UrlEncode(messageId)}", Headers());
        if (!resp.Ok) return null;
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null || (data["ok"] as JsonValue)?.GetValue<bool>() == false) return null;
        return data;
    }

    /// <summary>创建邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{ApiBase}/new", Headers());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var email = data is null ? "" : Json.Str(data, "email").Trim();
        if (data is null || (data["ok"] as JsonValue)?.GetValue<bool>() != true || email.Length == 0 || !email.Contains('@'))
            throw new Exception("tempmailc: invalid new email response");
        return new EmailInfo("tempmailc", email);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("tempmailc: empty email");
        var resp = Http.Get($"{ApiBase}/inbox?email={WebUtility.UrlEncode(address)}", Headers());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null || (data["ok"] as JsonValue)?.GetValue<bool>() != true)
            throw new Exception("tempmailc: inbox response failed");
        var rows = data["messages"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var item in rows)
        {
            if (item is not JsonObject ro) continue;
            var messageId = Json.Str(ro, "id").Trim();
            var detail = messageId.Length > 0 ? FetchMessage(address, messageId) : null;
            var flat = detail is not null ? Json.ToDict(detail) : FlattenListMessage(ro, address);
            result.Add(Normalize.NormalizeEmail(flat, address));
        }
        return result;
    }
}
