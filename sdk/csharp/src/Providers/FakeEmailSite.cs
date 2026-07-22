using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>fake-email-site 渠道（fake-email.site）。JSON REST API，创建 + 轮询。</summary>
public static class FakeEmailSite
{
    private const string Base = "https://api.fake-email.site";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["Content-Type"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    };

    /// <summary>创建临时邮箱：POST 空 JSON 对象，解析 temp_email_addr。token = 邮箱地址</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Post($"{Base}/api/temporary-address", "{}", "application/json", Headers);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        if (data is null) throw new Exception("fake-email-site: 无效响应格式");
        var email = (data["temp_email_addr"]?.GetValue<string>() ?? "").Trim();
        if (email.Length == 0) throw new Exception("fake-email-site: 响应中未找到临时邮箱地址");
        return new EmailInfo("fake-email-site", email, email);
    }

    /// <summary>轮询收件箱：GET /api/inbox/poll?address=xxx</summary>
    public static List<Email> GetEmails(string email)
    {
        var e = email ?? "";
        var resp = Http.Get($"{Base}/api/inbox/poll?address={e}", Headers);
        if (resp.StatusCode == 404) return new List<Email>();
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data is null) return result;
        if (data["messages"] is not JsonArray rows) return result;
        foreach (var row in rows)
            if (row is JsonObject) result.Add(Normalize.NormalizeEmail(Json.ToDict(row), e));
        return result;
    }
}
