using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// DisposableMail.app 渠道 — https://disposablemail.app
/// 纯 REST JSON API，无认证。
/// 创建：POST /api/inbox 返回 address + token。
/// 收信：GET /api/inbox/emails?token={token}。
/// </summary>
public static class DisposablemailApp
{
    private const string Base = "https://disposablemail.app";

    private static Dictionary<string, string> Headers() => new()
    {
        ["Content-Type"] = "application/json",
        ["Accept"] = "application/json",
        ["Referer"] = $"{Base}/",
        ["Origin"] = Base,
    };

    /// <summary>创建 disposablemail.app 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Post($"{Base}/api/inbox", "{}", "application/json", Headers());
        resp.EnsureSuccess();

        var body = Json.Parse(resp.Body) as JsonObject;
        var address = Json.Str(body, "address");
        var token = Json.Str(body, "token");

        if (string.IsNullOrEmpty(address) || !address.Contains('@'))
            throw new Exception($"disposablemail-app: 返回的邮箱地址无效: {address}");
        if (string.IsNullOrEmpty(token))
            throw new Exception("disposablemail-app: 返回的 token 为空");

        return new EmailInfo("disposablemail-app", address, token);
    }

    /// <summary>获取 disposablemail.app 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("disposablemail-app: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("disposablemail-app: 邮箱地址不能为空");

        var resp = Http.Get($"{Base}/api/inbox/emails?token={WebUtility.UrlEncode(token)}",
            new Dictionary<string, string> { ["Accept"] = "application/json", ["Referer"] = $"{Base}/" });
        resp.EnsureSuccess();

        var body = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (body?["emails"] is not JsonArray rows) return result;

        foreach (var node in rows)
        {
            if (node is not JsonObject item) continue;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(item, "id"),
                ["from"] = FirstNonEmpty(Json.Str(item, "from"), Json.Str(item, "from_address"), Json.Str(item, "sender")),
                ["to"] = FirstNonEmpty(Json.Str(item, "to"), addr),
                ["subject"] = Json.Str(item, "subject"),
                ["text"] = FirstNonEmpty(Json.Str(item, "text"), Json.Str(item, "body_text")),
                ["html"] = FirstNonEmpty(Json.Str(item, "html"), Json.Str(item, "body_html"), Json.Str(item, "body")),
                ["date"] = FirstNonEmpty(Json.Str(item, "date"), Json.Str(item, "created_at"), Json.Str(item, "receivedAt")),
            }, addr));
        }
        return result;
    }

    private static string FirstNonEmpty(params string[] values)
    {
        foreach (var v in values) if (!string.IsNullOrEmpty(v)) return v;
        return "";
    }
}
