using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>tempmail.lol V2 渠道（POST /inbox/create）。支持指定域名。</summary>
public static class TempMailLol
{
    private const string BaseUrl = "https://api.tempmail.lol/v2";
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36",
        ["Origin"] = "https://tempmail.lol",
    };

    public static EmailInfo Generate(string? domain)
    {
        var payload = Json.Serialize(new Dictionary<string, object?> { ["domain"] = domain, ["captcha"] = null });
        var resp = Http.Post($"{BaseUrl}/inbox/create", payload, "application/json", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        var address = Json.Str(data, "address");
        var token = Json.Str(data, "token");
        if (string.IsNullOrEmpty(address) || string.IsNullOrEmpty(token))
            throw new Exception("Failed to generate email");
        return new EmailInfo("tempmail-lol", address, token);
    }

    public static List<Email> GetEmails(string? token, string email)
    {
        var resp = Http.Get($"{BaseUrl}/inbox?token={WebUtility.UrlEncode(token ?? "")}", Headers);
        resp.EnsureSuccess();
        var emails = (Json.Parse(resp.Body) as JsonObject)?["emails"] as JsonArray;
        var result = new List<Email>();
        if (emails is null) return result;
        foreach (var raw in emails)
            if (raw is not null)
                result.Add(Normalize.NormalizeEmail(Json.ToDict(raw), email));
        return result;
    }
}

/// <summary>tempmail.lol V1 渠道（GET /generate）。</summary>
public static class TempMailLolV2
{
    private const string ApiBase = "https://api.tempmail.lol";
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    };

    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{ApiBase}/generate", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        var address = Json.Str(data, "address");
        var token = Json.Str(data, "token");
        if (string.IsNullOrEmpty(address) || string.IsNullOrEmpty(token))
            throw new Exception("tempmail-lol-v2: missing address or token");
        return new EmailInfo("tempmail-lol-v2", address, token);
    }

    public static List<Email> GetEmails(string? token, string email)
    {
        var resp = Http.Get($"{ApiBase}/auth/{WebUtility.UrlEncode(token ?? "")}", Headers);
        resp.EnsureSuccess();
        var list = (Json.Parse(resp.Body) as JsonObject)?["email"] as JsonArray;
        var result = new List<Email>();
        if (list is null) return result;
        foreach (var item in list)
        {
            if (item is not JsonObject raw) continue;
            var body = Json.Str(raw, "body");
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = !string.IsNullOrEmpty(Json.Str(raw, "id")) ? Json.Str(raw, "id") : Json.Str(raw, "_id"),
                ["from"] = !string.IsNullOrEmpty(Json.Str(raw, "from")) ? Json.Str(raw, "from") : Json.Str(raw, "sender"),
                ["to"] = email,
                ["subject"] = Json.Str(raw, "subject"),
                ["text"] = !string.IsNullOrEmpty(body) ? body : Json.Str(raw, "text"),
                ["html"] = !string.IsNullOrEmpty(Json.Str(raw, "html")) ? Json.Str(raw, "html") : body,
                ["date"] = !string.IsNullOrEmpty(Json.Str(raw, "date")) ? Json.Str(raw, "date") : Json.Str(raw, "receivedAt"),
            }, email));
        }
        return result;
    }
}
