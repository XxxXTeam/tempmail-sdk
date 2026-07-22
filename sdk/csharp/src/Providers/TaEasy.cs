using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>ta-easy.com 临时邮箱 — https://api-endpoint.ta-easy.com（JSON API）。</summary>
public static class TaEasy
{
    private const string ApiBase = "https://api-endpoint.ta-easy.com";
    private const string Origin = "https://www.ta-easy.com";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) " +
                         "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        ["origin"] = Origin,
        ["referer"] = $"{Origin}/",
    };

    public static EmailInfo Generate()
    {
        var h = new Dictionary<string, string>(Headers) { ["Content-Length"] = "0" };
        var resp = Http.Post($"{ApiBase}/temp-email/address/new", "", null, h);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        var address = Json.Str(data, "address");
        var token = Json.Str(data, "token");
        if (Json.Str(data, "status") != "success" || string.IsNullOrEmpty(address) || string.IsNullOrEmpty(token))
        {
            var msg = Json.Str(data, "message");
            throw new Exception($"ta-easy: {(string.IsNullOrEmpty(msg) ? "create failed" : msg)}");
        }
        long? expires = null;
        if (data is JsonObject o && o.TryGetPropertyValue("expiresAt", out var ea)
            && ea is not null && long.TryParse(ea.ToString(), out var ev)) expires = ev;
        return new EmailInfo("ta-easy", address, token, expires);
    }

    public static List<Email> GetEmails(string email, string? token)
    {
        var payload = Json.Serialize(new Dictionary<string, object?> { ["token"] = token, ["email"] = email });
        var resp = Http.Post($"{ApiBase}/temp-email/inbox/list", payload, "application/json",
            new Dictionary<string, string>(Headers));
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body);
        if (Json.Str(data, "status") != "success")
        {
            var msg = Json.Str(data, "message");
            throw new Exception($"ta-easy: {(string.IsNullOrEmpty(msg) ? "inbox failed" : msg)}");
        }
        var result = new List<Email>();
        if ((data as JsonObject)?["data"] is not JsonArray rows) return result;
        foreach (var item in rows)
            if (item is not null) result.Add(Normalize.NormalizeEmail(Json.ToDict(item), email));
        return result;
    }
}
