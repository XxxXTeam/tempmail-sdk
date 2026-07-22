using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>CleanTempMail 渠道（cleantempmail.com）。GET JSON API，X-API-Key 鉴权。</summary>
public static class CleanTempMail
{
    private const string ApiBase = "https://cleantempmail.com/api";

    private static string ApiKey()
    {
        var key = (Environment.GetEnvironmentVariable("CLEANTEMPMAIL_API_KEY") ?? "").Trim();
        return key.Length > 0 ? key : "ct-test";
    }

    private static Dictionary<string, string> BuildHeaders() => new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0",
        ["X-API-Key"] = ApiKey(),
    };

    private static JsonObject GetJson(string path)
    {
        var resp = Http.Get($"{ApiBase}{path}", BuildHeaders());
        resp.EnsureSuccess();
        return Json.Parse(resp.Body) as JsonObject ?? new JsonObject();
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var data = GetJson("/generate-email");
        var payload = data["data"] as JsonObject;
        var email = "";
        if (payload is not null)
        {
            email = (payload["email"]?.GetValue<string>() ?? payload["mailbox"]?.GetValue<string>() ?? "").Trim();
        }
        if (email.Length == 0 || !email.Contains('@'))
            throw new Exception("cleantempmail: invalid generate-email response");
        return new EmailInfo("cleantempmail", email);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("cleantempmail: empty email");

        var data = GetJson($"/emails?email={WebUtility.UrlEncode(address)}");
        var payload = data["data"] as JsonObject;
        var rows = payload?["emails"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var item in rows)
            if (item is JsonObject) result.Add(Normalize.NormalizeEmail(Json.ToDict(item), address));
        return result;
    }
}
