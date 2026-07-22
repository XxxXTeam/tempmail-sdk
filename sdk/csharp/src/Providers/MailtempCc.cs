using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// MailTemp.cc 渠道 — https://mailtemp.cc
/// PHP POST form-urlencoded API，无需认证。域名 @neocea.com。
/// 创建：POST /api.php body action=inbox，返回 JSON 字符串格式的用户名。
/// 收信：POST /api.php body action=fetch&inbox=&last_id=0 取列表 → action=view&id=&inbox= 取详情。
/// token 存用户名（@ 前部分）。
/// </summary>
public static class MailtempCc
{
    private const string Base = "https://mailtemp.cc";
    private const string Domain = "neocea.com";

    private static Dictionary<string, string> Headers() => new()
    {
        ["Content-Type"] = "application/x-www-form-urlencoded",
        ["Accept"] = "*/*",
        ["Referer"] = $"{Base}/",
        ["Origin"] = Base,
    };

    /// <summary>创建 mailtemp.cc 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Post($"{Base}/api.php", "action=inbox", "application/x-www-form-urlencoded", Headers());
        resp.EnsureSuccess();

        // 返回值为 JSON 字符串格式（如 "vindictiverate"），需去掉引号
        var rawText = resp.Body.Trim();
        string username;
        if (Json.Parse(rawText) is JsonValue v && v.TryGetValue<string>(out var s)) username = s;
        else username = rawText;

        if (string.IsNullOrEmpty(username))
            throw new Exception($"mailtemp-cc: 返回的用户名无效: {rawText}");

        var address = $"{username}@{Domain}";
        return new EmailInfo("mailtemp-cc", address, username);
    }

    /// <summary>获取 mailtemp.cc 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("mailtemp-cc: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("mailtemp-cc: 邮箱地址不能为空");

        var resp = Http.Post($"{Base}/api.php", $"action=fetch&inbox={token}&last_id=0",
            "application/x-www-form-urlencoded", Headers());
        resp.EnsureSuccess();

        var result = new List<Email>();
        if (Json.Parse(resp.Body) is not JsonArray rawList) return result;

        foreach (var node in rawList)
        {
            if (node is not JsonObject item) continue;
            var mailId = Json.Str(item, "id");
            if (mailId.Length == 0) continue;

            var detailResp = Http.Post($"{Base}/api.php", $"action=view&id={mailId}&inbox={token}",
                "application/x-www-form-urlencoded", Headers());
            detailResp.EnsureSuccess();
            var detail = Json.Parse(detailResp.Body) as JsonObject ?? new JsonObject();

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = mailId,
                ["from"] = FirstNonEmpty(Json.Str(item, "sender_email"), Json.Str(item, "sender")),
                ["to"] = addr,
                ["subject"] = FirstNonEmpty(Json.Str(item, "subject"), Json.Str(detail, "subject")),
                ["text"] = "",
                ["html"] = Json.Str(detail, "body_html"),
                ["date"] = FirstNonEmpty(Json.Str(item, "received_at"), Json.Str(detail, "received_at")),
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
