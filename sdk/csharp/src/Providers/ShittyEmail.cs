using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>shitty.email 渠道 — https://shitty.email（X-Session-Token 鉴权）。</summary>
public static class ShittyEmail
{
    private const string ApiBase = "https://shitty.email/api";

    private static Dictionary<string, string> Headers(string? token = null)
    {
        var h = new Dictionary<string, string>
        {
            ["Accept"] = "application/json",
            ["User-Agent"] = "Mozilla/5.0",
        };
        if (!string.IsNullOrEmpty(token)) h["X-Session-Token"] = token;
        return h;
    }

    public static EmailInfo Generate()
    {
        var resp = Http.Post($"{ApiBase}/inbox", null, null, Headers());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        var email = Json.Str(data, "email").Trim();
        var token = Json.Str(data, "token").Trim();
        if (string.IsNullOrEmpty(email) || !email.Contains('@') || string.IsNullOrEmpty(token))
            throw new Exception("shitty-email: invalid inbox response");
        long? expires = null;
        if (data is JsonObject o && o.TryGetPropertyValue("expiresAt", out var ea)
            && ea is not null && long.TryParse(ea.ToString(), out var ev)) expires = ev;
        return new EmailInfo("shitty-email", email, token, expires);
    }

    public static List<Email> GetEmails(string? token, string email)
    {
        var sessionToken = (token ?? "").Trim();
        var address = (email ?? "").Trim();
        if (string.IsNullOrEmpty(sessionToken)) throw new Exception("shitty-email: empty token");
        if (string.IsNullOrEmpty(address)) throw new Exception("shitty-email: empty email");

        var resp = Http.Get($"{ApiBase}/inbox", Headers(sessionToken));
        resp.EnsureSuccess();
        var rows = (Json.Parse(resp.Body) as JsonObject)?["emails"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var item in rows)
        {
            if (item is not JsonObject row) continue;
            var messageId = Json.Str(row, "id").Trim();
            var detail = string.IsNullOrEmpty(messageId) ? null : FetchMessage(sessionToken, messageId);
            var src = detail ?? row;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(src, "id"),
                ["from"] = Json.Str(src, "from"),
                ["to"] = string.IsNullOrEmpty(Json.Str(src, "to")) ? address : Json.Str(src, "to"),
                ["subject"] = Json.Str(src, "subject"),
                ["text"] = Json.Str(src, "text"),
                ["html"] = Json.Str(src, "html"),
                ["date"] = Json.Str(src, "date"),
            }, address));
        }
        return result;
    }

    private static JsonObject? FetchMessage(string token, string messageId)
    {
        try
        {
            var resp = Http.Get($"{ApiBase}/email/{WebUtility.UrlEncode(messageId)}", Headers(token));
            if (!resp.Ok) return null;
            return Json.Parse(resp.Body) as JsonObject;
        }
        catch { return null; }
    }
}
