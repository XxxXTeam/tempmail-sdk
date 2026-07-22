using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// BestTempMail 渠道 — https://best-temp-mail.com
/// 纯 JSON REST API，无认证。客户端生成 UUID 作为 intToken。
/// 创建：POST /api/v3/createEmail。收信：POST /api/v3/getEmailList。
/// token 存 JSON {"intToken","id","update_tag"}。
/// </summary>
public static class BestTempMail
{
    private const string Base = "https://best-temp-mail.com";

    private static Dictionary<string, string> Headers() => new()
    {
        ["Content-Type"] = "application/json",
        ["Referer"] = $"{Base}/",
        ["Origin"] = Base,
    };

    /// <summary>创建 best-temp-mail 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var intToken = Guid.NewGuid().ToString();
        var payload = Json.Serialize(new Dictionary<string, object?> { ["intToken"] = intToken });
        var resp = Http.Post($"{Base}/api/v3/createEmail", payload, "application/json", Headers());
        resp.EnsureSuccess();

        var body = Json.Parse(resp.Body) as JsonObject;
        if (Json.Str(body, "status") != "success")
            throw new Exception($"best-temp-mail: 创建邮箱失败: {resp.Body}");

        var data = body?["data"] as JsonObject;
        var address = Json.Str(data, "address");
        var id = Json.Str(data, "id");
        var updateTag = Json.Str(data, "update_tag");

        if (string.IsNullOrEmpty(address) || !address.Contains('@'))
            throw new Exception($"best-temp-mail: 返回的邮箱地址无效: {address}");

        var token = Json.Serialize(new Dictionary<string, object?>
        {
            ["intToken"] = intToken,
            ["id"] = id,
            ["update_tag"] = updateTag,
        });
        return new EmailInfo("best-temp-mail", address, token);
    }

    /// <summary>获取 best-temp-mail 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("best-temp-mail: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("best-temp-mail: 邮箱地址不能为空");

        var tok = Json.Parse(token) as JsonObject ?? throw new Exception("best-temp-mail: 无效的 token");
        var intToken = Json.Str(tok, "intToken");
        var id = Json.Str(tok, "id");
        var updateTag = Json.Str(tok, "update_tag");
        if (string.IsNullOrEmpty(intToken) || string.IsNullOrEmpty(id))
            throw new Exception("best-temp-mail: token 中缺少必要字段 (intToken / id)");

        var payload = Json.Serialize(new Dictionary<string, object?>
        {
            ["address"] = addr,
            ["id"] = id,
            ["intToken"] = intToken,
            ["update_tag"] = updateTag,
        });
        var resp = Http.Post($"{Base}/api/v3/getEmailList", payload, "application/json", Headers());
        resp.EnsureSuccess();

        var body = Json.Parse(resp.Body) as JsonObject;
        var data = body?["data"] as JsonObject;
        var hasNew = data?["hasNewEmail"]?.GetValue<bool>() ?? false;
        var result = new List<Email>();
        if (!hasNew) return result;

        if (data?["emails"] is not JsonArray rows) return result;
        foreach (var node in rows)
        {
            if (node is not JsonObject item) continue;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(item, "id"),
                ["from"] = FirstNonEmpty(Json.Str(item, "from"), Json.Str(item, "from_address"), Json.Str(item, "sender")),
                ["to"] = addr,
                ["subject"] = Json.Str(item, "subject"),
                ["text"] = FirstNonEmpty(Json.Str(item, "text"), Json.Str(item, "body_text")),
                ["html"] = FirstNonEmpty(Json.Str(item, "html"), Json.Str(item, "body_html"), Json.Str(item, "body")),
                ["date"] = FirstNonEmpty(Json.Str(item, "date"), Json.Str(item, "created_at")),
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
