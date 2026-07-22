using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// tempemail.info 渠道 — https://tempemail.info（PHP session）。
/// 创建：GET / 获取 PHPSESSID cookie，并从 HTML 正则提取 base64 邮箱地址。token 为 cookie 头串。
/// 收信：POST /template/checker.php（body: last_id=0）取列表，逐封 GET /view/{date} 取正文。
/// </summary>
public static class TempEmailInfo
{
    private const string BaseUrl = "https://tempemail.info";
    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";
    private static readonly Regex EmailRe = new("var\\s+emailEncoded\\s*=\\s*\"([^\"]+)\"", RegexOptions.Compiled);

    private static Dictionary<string, string> BaseHeaders() => new()
    {
        ["User-Agent"] = Ua,
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Referer"] = $"{BaseUrl}/",
        ["Origin"] = BaseUrl,
    };

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var headers = BaseHeaders();
        headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
        var resp = Http.Get($"{BaseUrl}/", headers);
        resp.EnsureSuccess();

        var cookieHdr = ProviderHttpUtil.CookieHeaderFrom(resp);
        if (string.IsNullOrEmpty(cookieHdr)) throw new Exception("tempemail-info: 未获取到会话 Cookie");

        var m = EmailRe.Match(resp.Body);
        if (!m.Success) throw new Exception("tempemail-info: 未在页面中找到 emailEncoded");
        string decoded;
        try { decoded = Encoding.UTF8.GetString(Convert.FromBase64String(m.Groups[1].Value)).Trim(); }
        catch { throw new Exception("tempemail-info: 邮箱地址 base64 解码失败"); }
        if (string.IsNullOrEmpty(decoded) || !decoded.Contains('@'))
            throw new Exception("tempemail-info: 解码出的邮箱地址无效");

        return new EmailInfo("tempemail-info", decoded, cookieHdr);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var cookieHdr = (token ?? "").Trim();
        if (string.IsNullOrEmpty(cookieHdr)) throw new Exception("tempemail-info: 缺少会话 Cookie");

        var headers = BaseHeaders();
        headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
        headers["X-Requested-With"] = "XMLHttpRequest";
        headers["Cookie"] = cookieHdr;

        var resp = Http.Post($"{BaseUrl}/template/checker.php", "last_id=0",
            "application/x-www-form-urlencoded", headers);
        resp.EnsureSuccess();

        var rows = Json.Parse(resp.Body) as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var r in rows)
        {
            if (r is not JsonObject row) continue;
            var fromAddr = Json.Str(row, "from");
            var name = Json.Str(row, "name");
            if (!string.IsNullOrEmpty(name) && name != fromAddr) fromAddr = $"{name} <{fromAddr}>";
            var date = Json.Str(row, "date");

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(row, "id"),
                ["from"] = fromAddr,
                ["to"] = email,
                ["subject"] = Json.Str(row, "subject"),
                ["date"] = date,
                ["html"] = FetchBody(cookieHdr, date),
                ["isRead"] = Truthy(row["read"]),
            }, email));
        }
        return result;
    }

    private static string FetchBody(string cookieHdr, string date)
    {
        if (string.IsNullOrEmpty(date.Trim())) return "";
        var headers = BaseHeaders();
        headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
        headers["Cookie"] = cookieHdr;
        HttpResult resp;
        try { resp = Http.Get($"{BaseUrl}/view/{WebUtility.UrlEncode(date)}", headers); }
        catch { return ""; }
        if (resp.StatusCode is < 200 or >= 300) return "";
        return resp.Body;
    }

    // read 字段可能是 bool、数字或字符串，统一判真
    private static bool Truthy(JsonNode? node)
    {
        if (node is not JsonValue v) return false;
        if (v.TryGetValue<bool>(out var b)) return b;
        if (v.TryGetValue<long>(out var l)) return l != 0;
        if (v.TryGetValue<double>(out var d)) return d != 0;
        if (v.TryGetValue<string>(out var s)) return s is "1" or "true";
        return false;
    }
}
