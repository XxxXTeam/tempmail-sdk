using System;
using System.Collections.Generic;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// restmail-net 渠道（restmail.net，Mozilla 开源项目）。ad-hoc 模式，无需创建。
/// 随机生成 username，邮箱即 username@restmail.net；收信 GET /mail/{username}。
/// </summary>
public static class RestmailNet
{
    private const string Base = "https://restmail.net";
    private static readonly Random Rand = new();

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
    };

    private static string RandomUsername()
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var length = Rand.Next(10, 13); // 10-12 位
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>创建 restmail.net 临时邮箱（本地随机 username）</summary>
    public static EmailInfo Generate()
    {
        return new EmailInfo("restmail-net", $"{RandomUsername()}@restmail.net");
    }

    /// <summary>获取 restmail.net 邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new Exception("restmail-net: 邮箱地址为空");
        var username = address.Split('@')[0];
        if (username.Length == 0) throw new Exception("restmail-net: 无法提取用户名");

        var resp = Http.Get($"{Base}/mail/{username}", Headers());
        if (resp.StatusCode == 404) return new List<Email>();
        resp.EnsureSuccess();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var item in rows)
        {
            if (item is not JsonObject msg) continue;
            var fromAddr = "";
            if (msg["from"] is JsonArray fromList && fromList.Count > 0 && fromList[0] is JsonObject f0)
                fromAddr = Json.Str(f0, "address");
            if (string.IsNullOrEmpty(fromAddr) && msg["headers"] is JsonObject headers)
                fromAddr = Json.Str(headers, "from");

            var raw = new Dictionary<string, object?>
            {
                ["id"] = Json.Str(msg, "id"),
                ["from"] = fromAddr,
                ["to"] = address,
                ["subject"] = Json.Str(msg, "subject"),
                ["text"] = Json.Str(msg, "text"),
                ["html"] = Json.Str(msg, "html"),
                ["date"] = Json.Str(msg, "receivedAt"),
                ["is_read"] = false,
            };
            result.Add(Normalize.NormalizeEmail(raw, address));
        }
        return result;
    }
}
