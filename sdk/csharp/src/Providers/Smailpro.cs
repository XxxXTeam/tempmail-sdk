using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// smailpro 渠道（smailpro.com + api.sonjj.com）。
/// 两段式：GET /app/payload?url=目标API 取 JWT，再携带 JWT 调用 sonjj 接口（payload=JWT）。
/// token 仅占位。创建 /create；列表 /inbox；详情 /message（补全正文）。
/// </summary>
public static class Smailpro
{
    private const string Base = "https://smailpro.com";
    private const string ApiBase = "https://api.sonjj.com/v1/temp_email";

    private static Dictionary<string, string> Headers() => new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Referer"] = $"{Base}/",
    };

    /// <summary>获取访问 sonjj API 所需的 JWT；extra 为附加查询参数（email、mid）</summary>
    private static string FetchPayload(string targetUrl, IEnumerable<KeyValuePair<string, string>>? extra)
    {
        var url = $"{Base}/app/payload?url={WebUtility.UrlEncode(targetUrl)}";
        if (extra is not null)
            foreach (var kv in extra)
                url += $"&{WebUtility.UrlEncode(kv.Key)}={WebUtility.UrlEncode(kv.Value)}";
        var resp = Http.Get(url, Headers());
        resp.EnsureSuccess();
        var payload = (resp.Body ?? "").Trim().Trim('"');
        if (payload.Length == 0) throw new Exception("smailpro: payload 为空");
        return payload;
    }

    /// <summary>携带 JWT 调用 sonjj API 并返回解析后的 JSON 节点</summary>
    private static JsonNode? CallApi(string targetUrl, IEnumerable<KeyValuePair<string, string>>? extra)
    {
        var payload = FetchPayload(targetUrl, extra);
        var resp = Http.Get($"{targetUrl}?payload={WebUtility.UrlEncode(payload)}", Headers());
        resp.EnsureSuccess();
        return Json.Parse(resp.Body);
    }

    /// <summary>创建 smailpro 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var data = CallApi($"{ApiBase}/create", null) as JsonObject
            ?? throw new Exception("smailpro: 创建响应格式异常");
        var email = Json.Str(data, "email").Trim();
        if (email.Length == 0) throw new Exception("smailpro: 创建邮箱失败, 未返回邮箱地址");
        return new EmailInfo("smailpro", email, email);
    }

    /// <summary>获取单封邮件正文，返回 (html, text)；失败返回空串</summary>
    private static (string html, string text) FetchMessage(string email, string mid)
    {
        if (mid.Length == 0) return ("", "");
        try
        {
            var data = CallApi($"{ApiBase}/message",
                new[] { new KeyValuePair<string, string>("email", email), new KeyValuePair<string, string>("mid", mid) }) as JsonObject;
            if (data is null) return ("", "");
            return (Json.Str(data, "body"), Json.Str(data, "textBody"));
        }
        catch { return ("", ""); }
    }

    /// <summary>获取 smailpro 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("smailpro: 邮箱地址为空");

        var data = CallApi($"{ApiBase}/inbox",
            new[] { new KeyValuePair<string, string>("email", addr) }) as JsonObject
            ?? throw new Exception("smailpro: 邮件列表响应格式异常");

        var messages = (data["data"] as JsonObject)?["messages"] as JsonArray ?? data["messages"] as JsonArray;
        var result = new List<Email>();
        if (messages is null || messages.Count == 0) return result;
        foreach (var m in messages)
        {
            if (m is not JsonObject mo) continue;
            var mid = Json.Str(mo, "mid").Trim();
            var (html, text) = FetchMessage(addr, mid);
            var raw = new Dictionary<string, object?>
            {
                ["id"] = mid,
                ["from"] = Json.Str(mo, "from"),
                ["to"] = addr,
                ["subject"] = Json.Str(mo, "subject"),
                ["date"] = Json.Str(mo, "datetime"),
            };
            if (html.Length > 0 || text.Length > 0)
            {
                raw["html"] = html;
                raw["text"] = text;
            }
            result.Add(Normalize.NormalizeEmail(raw, addr));
        }
        return result;
    }
}
