using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Maildrop 渠道 — https://maildrop.cx（REST JSON API，与 maildrop-cc GraphQL 独立）。
/// 创建：GET /api/suffixes.php 取域名（排除 transformer.edu.kg），随机本地部分拼邮箱。
/// 收信：GET /api/emails.php?addr=&page=1&limit=20 取 emails 数组。
/// token 存邮箱地址本身。
/// </summary>
public static class Maildrop
{
    private const string Base = "https://maildrop.cx";
    private const string ExcludedSuffix = "transformer.edu.kg";
    private static readonly Random Rand = new();

    private static Dictionary<string, string> Headers() => new()
    {
        ["Accept"] = "application/json, text/plain, */*",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["Cache-Control"] = "no-cache",
        ["DNT"] = "1",
        ["Pragma"] = "no-cache",
        ["Referer"] = "https://maildrop.cx/zh-cn/app",
        ["sec-ch-ua"] = "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        ["sec-ch-ua-mobile"] = "?0",
        ["sec-ch-ua-platform"] = "\"Windows\"",
        ["sec-fetch-dest"] = "empty",
        ["sec-fetch-mode"] = "cors",
        ["sec-fetch-site"] = "same-origin",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        ["x-requested-with"] = "XMLHttpRequest",
    };

    private static string RandomLocal(int length = 10)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static List<string> FetchSuffixes()
    {
        var resp = Http.Get($"{Base}/api/suffixes.php", Headers());
        resp.EnsureSuccess();
        if (Json.Parse(resp.Body) is not JsonArray data)
            throw new Exception("maildrop: invalid suffixes response");

        var ex = ExcludedSuffix.ToLowerInvariant();
        var result = new List<string>();
        foreach (var node in data)
        {
            if (node is not JsonValue v || !v.TryGetValue<string>(out var s)) continue;
            var t = s.Trim();
            if (t.Length > 0 && t.ToLowerInvariant() != ex) result.Add(t);
        }
        if (result.Count == 0) throw new Exception("maildrop: no domains available");
        return result;
    }

    private static string PickSuffix(List<string> suffixes, string? preferred)
    {
        if (!string.IsNullOrWhiteSpace(preferred))
        {
            var p = preferred.Trim().ToLowerInvariant();
            foreach (var d in suffixes)
                if (d.ToLowerInvariant() == p) return d;
            throw new Exception($"maildrop: domain not available: {p}");
        }
        return suffixes[Rand.Next(suffixes.Count)];
    }

    private static string CxDateToIso(string s)
    {
        s = (s ?? "").Trim();
        if (s.Length == 19 && s[10] == ' ')
            return s[..10] + "T" + s[11..] + "Z";
        return s;
    }

    /// <summary>创建 maildrop 临时邮箱（域名来自 suffixes，排除 transformer.edu.kg）</summary>
    public static EmailInfo Generate(string? domain)
    {
        var suffixes = FetchSuffixes();
        var dom = PickSuffix(suffixes, domain);
        var email = $"{RandomLocal()}@{dom}";
        return new EmailInfo("maildrop", email, email);
    }

    /// <summary>
    /// 通过详情接口获取单封邮件完整内容（GET /api/email_content.php?id={id}）。
    /// 详情响应字段（从前端代码确认）:
    ///   content: 完整 HTML 正文
    ///   subject / from_addr / date: 邮件元数据
    ///   attachment: JSON 字符串数组 [{filename, path, size}]（可能为空）
    /// </summary>
    private static JsonObject? FetchDetail(string id)
    {
        if (string.IsNullOrWhiteSpace(id)) return null;
        try
        {
            var resp = Http.Get($"{Base}/api/email_content.php?id={WebUtility.UrlEncode(id)}", Headers());
            if (resp.StatusCode < 200 || resp.StatusCode >= 300) return null;
            return Json.Parse(resp.Body) as JsonObject;
        }
        catch { return null; }
    }

    /// <summary>解析详情接口的 attachment 字段（JSON 字符串）为附件列表。</summary>
    private static List<EmailAttachment> ParseAttachments(string? raw)
    {
        var out_ = new List<EmailAttachment>();
        if (string.IsNullOrWhiteSpace(raw)) return out_;
        try
        {
            if (Json.Parse(raw) is not JsonArray arr) return out_;
            foreach (var node in arr)
            {
                if (node is not JsonObject obj) continue;
                var filename = Json.Str(obj, "filename").Trim();
                if (filename.Length == 0) continue;
                long? size = null;
                if (obj["size"] is JsonValue sv && sv.TryGetValue<long>(out var sz)) size = sz;
                out_.Add(new EmailAttachment { Filename = filename, Size = size ?? 0 });
            }
        }
        catch { /* 解析失败返回空列表 */ }
        return out_;
    }

    /// <summary>
    /// 获取 maildrop 邮件列表并对每封邮件补拉详情。
    /// 1. GET /api/emails.php 拉取列表（仅含 description 摘要）
    /// 2. 对每封邮件 GET /api/email_content.php?id={id} 拉取详情（含 content 完整 HTML）
    /// 3. 详情失败时保留列表 description 作为回退
    /// </summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) addr = (token ?? "").Trim();
        if (addr.Length == 0) throw new Exception("maildrop: empty address");

        var qs = $"addr={WebUtility.UrlEncode(addr)}&page=1&limit=20";
        var resp = Http.Get($"{Base}/api/emails.php?{qs}", Headers());
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data?["emails"] is not JsonArray rows) return result;

        foreach (var node in rows)
        {
            if (node is not JsonObject row) continue;
            var id = Json.Str(row, "id");
            var desc = Json.Str(row, "description").Trim();
            var ir = row["isRead"];
            var isRead = (ir is JsonValue b && b.TryGetValue<bool>(out var bv) && bv)
                || (ir is JsonValue n && n.TryGetValue<int>(out var nv) && nv == 1);

            var from = Json.Str(row, "from_addr").Trim();
            var subject = Json.Str(row, "subject").Trim();
            var date = CxDateToIso(Json.Str(row, "date"));
            var html = "";
            var attachments = new List<EmailAttachment>();

            /* 拉取详情覆盖 html/from/subject/date/attachments */
            if (!string.IsNullOrEmpty(id))
            {
                var detail = FetchDetail(id);
                if (detail is not null)
                {
                    var dContent = Json.Str(detail, "content");
                    if (!string.IsNullOrWhiteSpace(dContent)) html = dContent;
                    var dFrom = Json.Str(detail, "from_addr").Trim();
                    if (dFrom.Length > 0) from = dFrom;
                    var dSubj = Json.Str(detail, "subject").Trim();
                    if (dSubj.Length > 0) subject = dSubj;
                    var dDate = Json.Str(detail, "date");
                    if (!string.IsNullOrWhiteSpace(dDate)) date = CxDateToIso(dDate);
                    attachments = ParseAttachments(Json.Str(detail, "attachment"));
                }
            }

            result.Add(new Email
            {
                Id = id,
                From = from,
                To = addr,
                Subject = subject,
                Text = desc,
                Html = html,
                Date = date,
                IsRead = isRead,
                Attachments = attachments,
            });
        }
        return result;
    }
}
