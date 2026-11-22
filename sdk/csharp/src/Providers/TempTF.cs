using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// temp.tf — https://temp.tf
/// 创建返回一次性地址；收件列表响应按 bodyContentType 归位正文。
/// </summary>
public static class TempTF
{
    private const string ApiBase = "https://temp.tf/api";

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json",
        ["Content-Type"] = "application/json",
    };

    /// <summary>获取一个一次性地址</summary>
    public static EmailInfo Generate(string channel)
    {
        var resp = Http.Get($"{ApiBase}/account?plus=1", Headers());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var address = data is null ? "" : Json.Str(data, "email").Trim();
        if (address.Length == 0) throw new Exception("temp-tf: empty email");
        return new EmailInfo(channel, address);
    }

    /// <summary>获取 temp.tf 邮件，body 按 bodyContentType 归位</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("temp-tf: empty email");

        var payload = $"{{\"email\":\"{address}\"}}";
        var resp = Http.Post($"{ApiBase}/check", payload, "application/json", Headers());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var rows = data?["data"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var item in rows)
        {
            if (item is not JsonObject io) continue;
            var isText = "text".Equals(Json.Str(io, "bodyContentType"), StringComparison.OrdinalIgnoreCase);
            var body = Json.Str(io, "body");
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(io, "id"),
                ["from"] = Json.Str(io, "from"),
                ["to"] = address,
                ["subject"] = Json.Str(io, "subject"),
                ["text"] = isText ? body : "",
                ["html"] = isText ? "" : body,
                ["date"] = Json.Str(io, "date"),
                ["attachments"] = Json.ToRaw(io["attachments"]),
            }, address));
        }
        return result;
    }
}
