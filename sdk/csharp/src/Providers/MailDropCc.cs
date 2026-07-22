using System;
using System.Collections.Generic;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// maildrop.cc 渠道 — GraphQL 公共邮箱，无认证。
/// 创建邮箱：随机用户名 + @maildrop.cc（token 仅占位）。
/// 获取邮件：inbox(mailbox) 查列表，再逐封 message(mailbox,id) 补全 html 正文。
/// </summary>
public static class MailDropCc
{
    private const string Channel = "maildrop-cc";
    private const string Domain = "maildrop.cc";
    private const string GraphqlUrl = "https://api.maildrop.cc/graphql";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["Content-Type"] = "application/json",
        ["Origin"] = "https://maildrop.cc",
        ["Referer"] = "https://maildrop.cc/",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private const string LocalChars = "abcdefghijklmnopqrstuvwxyz0123456789";
    private static readonly Random Rand = new();

    private static string RandomLocal(int n = 10)
    {
        var sb = new StringBuilder();
        for (var i = 0; i < n; i++) sb.Append(LocalChars[Rand.Next(LocalChars.Length)]);
        return sb.ToString();
    }

    private static string Mailbox(string email)
    {
        email = (email ?? "").Trim();
        var idx = email.IndexOf('@');
        return idx >= 0 ? email[..idx] : email;
    }

    /// <summary>JSON 字符串字面量（含引号），用于安全嵌入 GraphQL 查询</summary>
    private static string JsonStr(string s) => Json.Serialize(s);

    private static JsonObject DoGraphql(string query)
    {
        var resp = Http.Post(GraphqlUrl, Json.Serialize(new Dictionary<string, object?> { ["query"] = query }),
            "application/json", Headers);
        resp.EnsureSuccess();
        return Json.Parse(resp.Body) as JsonObject ?? new JsonObject();
    }

    /// <summary>创建临时邮箱（无需 API 调用）</summary>
    public static EmailInfo Generate()
    {
        var email = $"{RandomLocal(10)}@{Domain}";
        return new EmailInfo(Channel, email, email);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var mailbox = Mailbox(email);
        if (mailbox.Length == 0) throw new ArgumentException("maildrop-cc: 邮箱地址为空");

        var inboxQuery = $"query {{ inbox(mailbox: {JsonStr(mailbox)}) {{ id headerfrom subject date }} }}";
        var inboxResp = DoGraphql(inboxQuery);
        var items = (inboxResp["data"] as JsonObject)?["inbox"] as JsonArray;
        var result = new List<Email>();
        if (items is null) return result;

        foreach (var it in items)
        {
            if (it is not JsonObject item) continue;
            var mid = Json.Str(item, "id");

            var raw = new Dictionary<string, object?>
            {
                ["id"] = mid,
                ["from"] = Json.Str(item, "headerfrom"),
                ["subject"] = Json.Str(item, "subject"),
                ["date"] = Json.Str(item, "date"),
            };

            if (mid.Length > 0)
            {
                try
                {
                    var msgQuery = $"query {{ message(mailbox: {JsonStr(mailbox)}, id: {JsonStr(mid)}) {{ id headerfrom subject date html }} }}";
                    var msgResp = DoGraphql(msgQuery);
                    if ((msgResp["data"] as JsonObject)?["message"] is JsonObject msg)
                        raw = new Dictionary<string, object?>
                        {
                            ["id"] = string.IsNullOrEmpty(Json.Str(msg, "id")) ? mid : Json.Str(msg, "id"),
                            ["from"] = Json.Str(msg, "headerfrom"),
                            ["subject"] = Json.Str(msg, "subject"),
                            ["date"] = Json.Str(msg, "date"),
                            ["html"] = Json.Str(msg, "html"),
                        };
                }
                catch { /* 回退列表摘要 */ }
            }

            result.Add(Normalize.NormalizeEmail(raw, email));
        }
        return result;
    }
}
