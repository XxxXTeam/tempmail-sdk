using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// temp-mail.org 渠道 — https://temp-mail.org（API: https://web2.temp-mail.org）。
/// 创建：POST /mailbox 返回 token(JWT) 与 mailbox 地址。
/// 收信：GET /messages 列表 + 逐封 GET /messages/{id} 取 HTML 正文，Bearer 鉴权。
/// </summary>
public static class TempMailOrg
{
    private const string BaseUrl = "https://web2.temp-mail.org";

    private static Dictionary<string, string> BaseHeaders() => new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        ["Origin"] = "https://temp-mail.org",
        ["Referer"] = "https://temp-mail.org/",
    };

    private static Dictionary<string, string> AuthHeaders(string? token)
    {
        var h = BaseHeaders();
        h["Authorization"] = $"Bearer {token}";
        return h;
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Post($"{BaseUrl}/mailbox", null, null, BaseHeaders());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var token = Json.Str(data, "token");
        var mailbox = Json.Str(data, "mailbox");
        if (string.IsNullOrEmpty(token) || string.IsNullOrEmpty(mailbox))
            throw new System.Exception("temp-mail-org: 创建邮箱失败");
        return new EmailInfo("temp-mail-org", mailbox, token);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new System.Exception("temp-mail-org: token 不能为空");
        var addr = (email ?? "").Trim();
        if (string.IsNullOrEmpty(addr)) throw new System.Exception("temp-mail-org: 邮箱地址不能为空");

        var resp = Http.Get($"{BaseUrl}/messages", AuthHeaders(token));
        resp.EnsureSuccess();
        var messages = (Json.Parse(resp.Body) as JsonObject)?["messages"] as JsonArray;
        var result = new List<Email>();
        if (messages is null) return result;

        foreach (var m in messages)
        {
            if (m is not JsonObject msg) continue;
            var id = Json.Str(msg, "_id");
            if (string.IsNullOrEmpty(id)) continue;
            var detail = FetchDetail(token, id);
            var src = detail ?? msg;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = id,
                ["from"] = !string.IsNullOrEmpty(Json.Str(src, "from")) ? Json.Str(src, "from") : Json.Str(msg, "from"),
                ["to"] = addr,
                ["subject"] = !string.IsNullOrEmpty(Json.Str(src, "subject")) ? Json.Str(src, "subject") : Json.Str(msg, "subject"),
                ["text"] = !string.IsNullOrEmpty(Json.Str(src, "bodyPreview")) ? Json.Str(src, "bodyPreview") : Json.Str(msg, "bodyPreview"),
                ["html"] = Json.Str(src, "bodyHtml"),
                ["date"] = !string.IsNullOrEmpty(Json.Str(src, "createdAt")) ? Json.Str(src, "createdAt") : Json.Str(msg, "receivedAt"),
            }, addr));
        }
        return result;
    }

    private static JsonObject? FetchDetail(string? token, string id)
    {
        try
        {
            var resp = Http.Get($"{BaseUrl}/messages/{id}", AuthHeaders(token));
            if (!resp.Ok) return null;
            return Json.Parse(resp.Body) as JsonObject;
        }
        catch { return null; }
    }
}
