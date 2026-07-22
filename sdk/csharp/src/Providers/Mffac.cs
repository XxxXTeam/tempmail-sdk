using System;
using System.Collections.Generic;
using System.Globalization;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// MFFAC 渠道（www.mffac.com/api）。
/// 创建：POST /mailboxes（expiresInHours=24）；收信：GET /mailboxes/{local}/emails，逐封拉详情。
/// </summary>
public static class Mffac
{
    private const string Base = "https://www.mffac.com/api";

    private static Dictionary<string, string> PostHeaders() => new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        ["Accept"] = "*/*",
        ["Origin"] = "https://www.mffac.com",
        ["Referer"] = "https://www.mffac.com/",
    };

    private static Dictionary<string, string> GetHeaders() => PostHeaders();

    private static string ReceivedAtToIso(JsonNode? node)
    {
        if (node is null) return "";
        double seconds;
        if (node is JsonValue v)
        {
            if (v.TryGetValue<double>(out var d)) seconds = d;
            else if (v.TryGetValue<long>(out var l)) seconds = l;
            else if (v.TryGetValue<string>(out var s) && double.TryParse(s, NumberStyles.Any, CultureInfo.InvariantCulture, out var sr)) seconds = sr;
            else return "";
        }
        else return "";
        if (seconds <= 0) return "";
        return DateTimeOffset.FromUnixTimeSeconds((long)seconds).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ", CultureInfo.InvariantCulture);
    }

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient) => new()
    {
        ["id"] = Json.Str(raw, "id"),
        ["from"] = Json.Str(raw, "fromAddress"),
        ["to"] = string.IsNullOrEmpty(Json.Str(raw, "toAddress")) ? recipient : Json.Str(raw, "toAddress"),
        ["subject"] = Json.Str(raw, "subject"),
        ["text"] = Json.Str(raw, "textContent"),
        ["html"] = Json.Str(raw, "htmlContent"),
        ["date"] = ReceivedAtToIso(raw["receivedAt"]),
    };

    private static JsonObject? FetchDetail(string messageId)
    {
        var r = Http.Get($"{Base}/emails/{WebUtility.UrlEncode(messageId)}", GetHeaders());
        if (!r.Ok) return null;
        var data = Json.Parse(r.Body) as JsonObject;
        if (data is null || (data["success"] as JsonValue)?.GetValue<bool>() != true) return null;
        return data["email"] as JsonObject;
    }

    /// <summary>创建邮箱</summary>
    public static EmailInfo Generate()
    {
        var payload = Json.Serialize(new Dictionary<string, object?> { ["expiresInHours"] = 24 });
        var r = Http.Post($"{Base}/mailboxes", payload, "application/json", PostHeaders());
        r.EnsureSuccess();
        var data = Json.Parse(r.Body) as JsonObject;
        if (data is null || (data["success"] as JsonValue)?.GetValue<bool>() != true || data["mailbox"] is not JsonObject mb)
            throw new Exception("mffac: create failed");
        var addr = Json.Str(mb, "address").Trim();
        var mid = Json.Str(mb, "id").Trim();
        if (addr.Length == 0 || mid.Length == 0) throw new Exception("mffac: invalid mailbox");
        return new EmailInfo("mffac", $"{addr}@mffac.com", mid, createdAt: mb["createdAt"]?.ToString());
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var local = string.IsNullOrEmpty(email) ? "" : email.Split('@', 2)[0].Trim();
        if (local.Length == 0) throw new Exception("mffac: empty address");
        var r = Http.Get($"{Base}/mailboxes/{local}/emails", GetHeaders());
        r.EnsureSuccess();
        var data = Json.Parse(r.Body) as JsonObject;
        if (data is null || (data["success"] as JsonValue)?.GetValue<bool>() != true)
            throw new Exception("mffac: list failed");
        var rows = data["emails"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var raw in rows)
        {
            if (raw is not JsonObject ro) continue;
            var messageId = Json.Str(ro, "id").Trim();
            var detail = messageId.Length > 0 ? FetchDetail(messageId) : null;
            result.Add(Normalize.NormalizeEmail(Flatten(detail ?? ro, email), email));
        }
        return result;
    }
}
