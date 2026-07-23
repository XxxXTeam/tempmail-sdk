using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Mail Sunls 渠道（mail.sunls.de）。
/// 无需 token/session：从 /api/domain 取域名列表本地随机拼地址；GET /api/fetch?to= 收信。
/// </summary>
public static class MailSunls
{
    private const string Base = "https://mail.sunls.de";
    private static readonly Random Rand = new();

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Cache-Control"] = "no-cache",
        ["Pragma"] = "no-cache",
        ["Referer"] = "https://mail.sunls.de/",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static string RandomLocal(int length = 10)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>创建邮箱：取域名列表后本地随机生成地址</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{Base}/api/domain", Headers());
        resp.EnsureSuccess();
        var arr = Json.Parse(resp.Body) as JsonArray
            ?? throw new Exception("mail-sunls: 域名列表响应格式无效");
        var domains = new List<string>();
        foreach (var d in arr)
        {
            var s = (d as JsonValue)?.GetValue<string>()?.Trim();
            if (!string.IsNullOrEmpty(s)) domains.Add(s);
        }
        if (domains.Count == 0) throw new Exception("mail-sunls: 无可用域名");
        var domain = domains[Rand.Next(domains.Count)];
        return new EmailInfo("mail-sunls", $"{RandomLocal()}@{domain}");
    }

    /// <summary>
    /// 通过详情接口获取单封邮件完整正文（GET /api/fetch/{id}）。
    /// </summary>
    /// <param name="id">邮件 ID</param>
    /// <returns>详情 JsonObject，失败返回 null</returns>
    private static JsonObject? FetchDetail(string id)
    {
        if (string.IsNullOrWhiteSpace(id)) return null;
        try
        {
            var resp = Http.Get($"{Base}/api/fetch/{WebUtility.UrlEncode(id)}", Headers());
            if (resp.StatusCode < 200 || resp.StatusCode >= 300) return null;
            return Json.Parse(resp.Body) as JsonObject;
        }
        catch { return null; }
    }

    /// <summary>从列表条目提取邮件 ID（支持多字段）。</summary>
    private static string ExtractId(JsonObject row)
    {
        foreach (var key in new[] { "id", "_id", "mail_id", "messageId", "message_id" })
        {
            if (row[key] is null) continue;
            var s = Json.Str(row, key).Trim();
            if (s.Length > 0) return s;
        }
        return "";
    }

    /// <summary>
    /// 获取邮件列表。
    /// 1. GET /api/fetch?to={email} 拉取列表元数据
    /// 2. 对每封邮件 GET /api/fetch/{id} 拉取详情（含完整 text/html）
    /// 3. 详情失败时保留列表字段作为回退
    /// </summary>
    public static List<Email> GetEmails(string email)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("mail-sunls: 邮箱地址不能为空");
        var resp = Http.Get($"{Base}/api/fetch?to={WebUtility.UrlEncode(addr)}", Headers());
        resp.EnsureSuccess();
        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;
        foreach (var raw in rows)
        {
            if (raw is not JsonObject row) continue;
            var id = ExtractId(row);
            /* 无条件调用详情接口，用详情字段覆盖列表字段（确保正文完整） */
            if (id.Length > 0)
            {
                var detail = FetchDetail(id);
                if (detail is not null)
                {
                    foreach (var kv in detail)
                    {
                        if (kv.Value is null) continue;
                        row[kv.Key] = kv.Value.DeepClone();
                    }
                }
            }
            result.Add(Normalize.NormalizeEmail(Json.ToDict(row), addr));
        }
        return result;
    }
}
