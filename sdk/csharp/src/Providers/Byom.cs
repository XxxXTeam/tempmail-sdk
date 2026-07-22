using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>Byom 渠道（byom.de）。纯 GET 无认证，本地生成随机用户名。</summary>
public static class Byom
{
    private const string Domain = "byom.de";
    private const string Base = "https://api.byom.de";
    private static readonly Random Rand = new();

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static string RandomUsername(int length = 10)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>生成随机邮箱地址，无需 API 调用</summary>
    public static EmailInfo Generate() => new("byom", $"{RandomUsername()}@{Domain}");

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var e = (email ?? "").Trim();
        if (e.Length == 0) throw new Exception("byom: empty email");
        var username = e.Split('@')[0];
        if (username.Length == 0) throw new Exception("byom: invalid email format");

        var resp = Http.Get($"{Base}/mails/{username}", Headers);
        resp.EnsureSuccess();
        var arr = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (arr is null) return result;
        foreach (var item in arr)
            if (item is JsonObject) result.Add(Normalize.NormalizeEmail(Json.ToDict(item), e));
        return result;
    }
}
