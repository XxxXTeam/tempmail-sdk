using System;
using System.Collections.Generic;
using System.Globalization;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// TempMail Fish 渠道 — https://tempmail.fish（API: https://api.tempmail.fish）。
/// 创建：POST /emails/new-email 返回 email 与 authKey，token 序列化保存 {"email","authKey"}。
/// 收信：GET /emails/emails?emailAddress={email}，鉴权头 Authorization: {authKey}（不带 Bearer）。
/// </summary>
public static class TempMailFish
{
    private const string ApiBase = "https://api.tempmail.fish";
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    };

    private static string ToIso(JsonNode? timestamp)
    {
        if (timestamp is not JsonValue v) return "";
        double millis;
        if (v.TryGetValue<double>(out var d)) millis = d;
        else if (v.TryGetValue<string>(out var s) && double.TryParse(s, NumberStyles.Any, CultureInfo.InvariantCulture, out var ps)) millis = ps;
        else return "";
        try { return DateTimeOffset.FromUnixTimeMilliseconds((long)millis).ToString("o", CultureInfo.InvariantCulture); }
        catch { return ""; }
    }

    private static Dictionary<string, object?> Flatten(JsonObject raw, string recipient) => new()
    {
        ["from"] = Json.Str(raw, "from"),
        ["to"] = !string.IsNullOrEmpty(Json.Str(raw, "to")) ? Json.Str(raw, "to") : recipient,
        ["subject"] = Json.Str(raw, "subject"),
        ["text"] = Json.Str(raw, "textBody"),
        ["html"] = Json.Str(raw, "htmlBody"),
        ["date"] = ToIso(raw["timestamp"]),
        ["is_read"] = false,
        ["attachments"] = raw["attachments"] is JsonArray a ? Json.ToRaw(a) : null,
    };

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var h = new Dictionary<string, string>(Headers) { ["Content-Type"] = "application/json" };
        var resp = Http.Post($"{ApiBase}/emails/new-email", null, "application/json", h);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var email = Json.Str(data, "email").Trim();
        var authKey = Json.Str(data, "authKey").Trim();
        if (string.IsNullOrEmpty(email) || !email.Contains('@') || string.IsNullOrEmpty(authKey))
            throw new Exception("tempmail-fish: 创建邮箱响应无效");
        var token = Json.Serialize(new Dictionary<string, object?> { ["email"] = email, ["authKey"] = authKey });
        return new EmailInfo("tempmail-fish", email, token);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("tempmail-fish: token 不能为空");
        var parsed = Json.Parse(token) as JsonObject;
        if (parsed is null) throw new Exception("tempmail-fish: token 格式无效");
        var address = Json.Str(parsed, "email").Trim();
        if (string.IsNullOrEmpty(address)) address = (email ?? "").Trim();
        var authKey = Json.Str(parsed, "authKey").Trim();
        if (string.IsNullOrEmpty(address) || string.IsNullOrEmpty(authKey))
            throw new Exception("tempmail-fish: token 缺少 email 或 authKey");

        var h = new Dictionary<string, string>(Headers) { ["Authorization"] = authKey };
        var resp = Http.Get($"{ApiBase}/emails/emails?emailAddress={WebUtility.UrlEncode(address)}", h);
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body);
        var rows = data as JsonArray ?? (data as JsonObject)?["emails"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var r in rows)
            if (r is JsonObject row)
                result.Add(Normalize.NormalizeEmail(Flatten(row, address), address));
        return result;
    }
}
