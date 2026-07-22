using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// linshiyouxiang.net 渠道 — https://www.linshiyouxiang.net
/// 创建：GET / 取首页 HTML，正则提取 tempMailGlobal（邮箱）与 mailCodeGlobal（校验 code）。
/// 收信：POST /get-messages  body JSON{"email","code"}，响应 {"emails":null|[...],"success"}。
/// token 存 mailCodeGlobal 值（HMAC 哈希，不依赖 cookie）。
/// </summary>
public static class LinshiyouxiangNet
{
    private const string BaseUrl = "https://www.linshiyouxiang.net";

    private static readonly Regex EmailRe = new("tempMailGlobal\\s*=\\s*'([^']+)'", RegexOptions.Compiled);
    private static readonly Regex CodeRe = new("mailCodeGlobal\\s*=\\s*'([^']+)'", RegexOptions.Compiled);

    private const string Ua =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static Dictionary<string, string> ApiHeaders() => new()
    {
        ["Accept"] = "application/json, text/javascript, */*; q=0.01",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Referer"] = $"{BaseUrl}/",
        ["Origin"] = BaseUrl,
        ["User-Agent"] = Ua,
        ["Content-Type"] = "application/json",
        ["X-Requested-With"] = "XMLHttpRequest",
    };

    /// <summary>创建 linshiyouxiang.net 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var headers = new Dictionary<string, string>
        {
            ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
            ["User-Agent"] = Ua,
        };
        var resp = Http.Get($"{BaseUrl}/", headers);
        resp.EnsureSuccess();
        var html = resp.Body;

        var m = EmailRe.Match(html);
        if (!m.Success) throw new Exception("linshiyouxiang-net: 未能从首页提取邮箱地址");
        var email = m.Groups[1].Value.Trim();
        if (email.Length == 0) throw new Exception("linshiyouxiang-net: 提取的邮箱地址为空");

        var cm = CodeRe.Match(html);
        var code = cm.Success ? cm.Groups[1].Value.Trim() : "";

        return new EmailInfo("linshiyouxiang-net", email, code);
    }

    /// <summary>获取 linshiyouxiang.net 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("linshiyouxiang-net: 邮箱地址为空");

        var payload = Json.Serialize(new Dictionary<string, object?> { ["email"] = addr, ["code"] = token ?? "" });
        var resp = Http.Post($"{BaseUrl}/get-messages", payload, "application/json", ApiHeaders());
        resp.EnsureSuccess();

        var result = Json.Parse(resp.Body) as JsonObject
            ?? throw new Exception("linshiyouxiang-net: 邮件列表响应格式异常");
        var list = result["emails"] as JsonArray;
        var output = new List<Email>();
        if (list is null || list.Count == 0) return output;

        foreach (var node in list)
        {
            if (node is not JsonObject item) continue;
            output.Add(Normalize.NormalizeEmail(Json.ToDict(item), addr));
        }
        return output;
    }
}
