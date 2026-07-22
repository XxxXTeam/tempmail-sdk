using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>HarakiriMail 渠道（harakirimail.com）。无认证 REST API。</summary>
public static class HarakiriMail
{
    private const string Base = "https://harakirimail.com";
    private static readonly Random Rand = new();

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static string RandomName(int length = 12)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new System.Text.StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var name = RandomName();
        var email = $"{name}@harakirimail.com";
        // 调用收件箱接口验证地址可用
        var resp = Http.Get($"{Base}/api/v1/inbox/{name}", Headers);
        resp.EnsureSuccess();
        return new EmailInfo("harakirimail", email);
    }

    private static JsonObject FetchBody(string emailId)
    {
        if (string.IsNullOrEmpty(emailId)) return new JsonObject();
        try
        {
            var resp = Http.Get($"{Base}/api/v1/email/{emailId}", Headers);
            resp.EnsureSuccess();
            return Json.Parse(resp.Body) as JsonObject ?? new JsonObject();
        }
        catch { return new JsonObject(); }
    }

    /// <summary>获取邮件列表，逐封拉取正文</summary>
    public static List<Email> GetEmails(string email)
    {
        var e = (email ?? "").Trim();
        if (e.Length == 0) throw new Exception("harakirimail: 邮箱地址为空");
        var name = e.Contains('@') ? e[..e.LastIndexOf('@')] : e;

        var resp = Http.Get($"{Base}/api/v1/inbox/{name}", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data is null) return result;
        if (data["emails"] is not JsonArray rows) return result;

        foreach (var r in rows)
        {
            if (r is not JsonObject raw) continue;
            var emailId = Pick(raw, "_id");
            var detail = FetchBody(emailId);
            var flat = new Dictionary<string, object?>
            {
                ["id"] = emailId,
                ["from"] = Pick(raw, "from"),
                ["to"] = e,
                ["subject"] = Pick(raw, "subject"),
                ["date"] = Pick(raw, "received"),
                ["html"] = Pick(detail, "body_html", "html"),
                ["text"] = Pick(detail, "body_text", "text"),
                ["isRead"] = false,
            };
            result.Add(Normalize.NormalizeEmail(flat, e));
        }
        return result;
    }

    private static string Pick(JsonObject o, params string[] keys)
    {
        foreach (var k in keys)
        {
            if (o[k] is JsonNode n)
            {
                var s = Json.NodeToString(n);
                if (!string.IsNullOrEmpty(s)) return s;
            }
        }
        return "";
    }
}
