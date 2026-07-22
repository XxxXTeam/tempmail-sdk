using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// TempGBox 渠道 — https://tempgbox.net。
/// POST /api/proxy?route={route} 转发；响应正文含 data-x="{base64 JSON}"。
/// 上游按 X-Device-ID 限流，每个新邮箱使用新的随机设备 ID（token 存设备 ID）。
/// </summary>
public static class TempGBox
{
    private const string Channel = "tempgbox";
    private const string ApiUrl = "https://tempgbox.net/api/proxy";
    private const string Origin = "https://tempgbox.net";

    private static readonly Dictionary<string, string> DefaultHeaders = new()
    {
        ["Accept"] = "text/html,application/json",
        ["Content-Type"] = "application/json",
        ["Origin"] = Origin,
        ["Referer"] = $"{Origin}/",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    };

    private static string RandomDeviceId()
    {
        var bytes = new byte[32];
        RandomNumberGenerator.Fill(bytes);
        return Convert.ToHexString(bytes).ToLowerInvariant();
    }

    private static string RandomIp()
    {
        return $"{RandomNumberGenerator.GetInt32(1, 255)}.{RandomNumberGenerator.GetInt32(1, 255)}.{RandomNumberGenerator.GetInt32(1, 255)}.{RandomNumberGenerator.GetInt32(1, 255)}";
    }

    private static JsonObject DecodePayload(string html)
    {
        var marker = "data-x=\"";
        var start = html.IndexOf(marker, StringComparison.Ordinal);
        var quote = '"';
        if (start < 0)
        {
            marker = "data-x='";
            start = html.IndexOf(marker, StringComparison.Ordinal);
            quote = '\'';
        }
        if (start < 0) throw new Exception("tempgbox: missing encoded response payload");
        start += marker.Length;
        var end = html.IndexOf(quote, start);
        if (end < 0) throw new Exception("tempgbox: malformed encoded response payload");
        var raw = Encoding.UTF8.GetString(Convert.FromBase64String(html[start..end]));
        return Json.Parse(raw) as JsonObject ?? new JsonObject();
    }

    private static JsonObject PostProxy(string route, string deviceId, object payload)
    {
        var ip = RandomIp();
        var headers = new Dictionary<string, string>(DefaultHeaders)
        {
            ["X-Device-ID"] = deviceId,
            ["X-Forwarded-For"] = ip,
            ["X-Real-IP"] = ip,
            ["X-Originating-IP"] = ip,
        };
        var resp = Http.Post($"{ApiUrl}?route={route}", Json.Serialize(payload), "application/json", headers);
        var data = DecodePayload(resp.Body);
        if (!resp.Ok)
        {
            var reason = Json.Str(data, "detail");
            if (reason.Length == 0) reason = Json.Str(data, "error");
            if (reason.Length == 0) reason = Json.Str(data, "message");
            throw new Exception($"tempgbox {route} failed: {resp.StatusCode} {reason}");
        }
        return data;
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var deviceId = RandomDeviceId();
        var data = PostProxy("generate", deviceId, new Dictionary<string, object?> { ["variant"] = "googlemail" });
        var alias = data["alias"] as JsonObject ?? new JsonObject();
        var email = Json.Str(alias, "email");
        if (email.Length == 0) email = Json.Str(alias, "alias");
        if (email.Length == 0) throw new Exception("tempgbox: missing email");

        long? expiresAt = null;
        var expStr = Json.Str(alias, "expires_at");
        if (expStr.Length > 0 && DateTimeOffset.TryParse(expStr.Replace("Z", "+00:00"), out var dto))
            expiresAt = dto.ToUnixTimeMilliseconds();

        return new EmailInfo(Channel, email, deviceId, expiresAt,
            createdAt: Json.Str(alias, "created_at") is { Length: > 0 } c ? c : null);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("tempgbox: missing device id");
        var data = PostProxy("inbox", token, new Dictionary<string, object?> { ["email"] = email });
        var messages = data["messages"] as JsonArray;
        var outList = new List<Email>();
        if (messages is null) return outList;
        foreach (var raw in messages)
        {
            if (raw is not JsonObject ro) continue;
            var flat = Json.ToDict(ro);
            flat["from"] = FirstNonEmpty(Json.Str(ro, "from"), Json.Str(ro, "sender"));
            flat["text"] = FirstNonEmpty(Json.Str(ro, "text"), Json.Str(ro, "body_text"));
            flat["html"] = FirstNonEmpty(Json.Str(ro, "html"), Json.Str(ro, "body_html"));
            flat["date"] = FirstNonEmpty(Json.Str(ro, "date"), Json.Str(ro, "received_at"));
            flat["messageId"] = FirstNonEmpty(Json.Str(ro, "messageId"), Json.Str(ro, "message_id"), Json.Str(ro, "id"));
            outList.Add(Normalize.NormalizeEmail(flat, email));
        }
        return outList;
    }

    private static string FirstNonEmpty(params string[] values)
    {
        foreach (var v in values) if (!string.IsNullOrEmpty(v)) return v;
        return "";
    }
}
