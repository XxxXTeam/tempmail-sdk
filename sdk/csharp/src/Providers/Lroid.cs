using System;
using System.Collections.Generic;
using System.Net;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Lroid 渠道 — https://lroid.com。
/// HTML 解析模式，服务端首次访问自动分配邮箱（域名 yevme.com），session cookie 维持身份。
/// 创建：GET / 提取 &lt;input id="eposta_adres"&gt;，cookie 序列化为 {"cookie":...} token。
/// 收信：优先 GET /en/api-kontrol/（JSON），失败回退 HTML 页面解析并逐封拉详情。
/// </summary>
public static class Lroid
{
    private const string Channel = "lroid";
    private const string BaseUrl = "https://lroid.com";
    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        ["Accept-Language"] = "en-US,en;q=0.9",
        ["Referer"] = $"{BaseUrl}/",
        ["User-Agent"] = Ua,
    };

    private static readonly Regex EmailRe = new(
        "<input[^>]+id=[\"']eposta_adres[\"'][^>]+value=[\"']([^\"']+@[^\"']+)[\"']", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex EmailReAlt = new(
        "<input[^>]+value=[\"']([^\"']+@[^\"']+)[\"'][^>]+id=[\"']eposta_adres[\"']", RegexOptions.IgnoreCase | RegexOptions.Compiled);

    private static string ExtractEmail(string html)
    {
        var match = EmailRe.Match(html);
        if (!match.Success) match = EmailReAlt.Match(html);
        if (!match.Success) throw new Exception("lroid: 无法从 HTML 响应中解析邮箱地址");
        var addr = match.Groups[1].Value.Trim();
        if (!addr.Contains('@')) throw new Exception("lroid: 解析到的邮箱地址无效");
        return addr;
    }

    private static string DecodeToken(string token)
    {
        JsonObject? data;
        try { data = Json.Parse(token) as JsonObject; }
        catch { throw new ArgumentException("lroid: 无效的 session token"); }
        var cookie = Json.Str(data, "cookie").Trim();
        if (cookie.Length == 0) throw new ArgumentException("lroid: session token 中缺少 cookie");
        return cookie;
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get(BaseUrl, Headers);
        resp.EnsureSuccess();
        var email = ExtractEmail(resp.Body);
        var cookie = ProviderHttpUtil.CookieHeaderFrom(resp);
        var token = Json.Serialize(new Dictionary<string, object?> { ["cookie"] = cookie });
        return new EmailInfo(Channel, email, token);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var cookie = DecodeToken(token ?? "");
        var address = (email ?? "").Trim();
        if (address.Length == 0) throw new ArgumentException("lroid: 邮箱地址不能为空");

        // 尝试 kontrol JSON API
        try
        {
            var resp = Http.Get($"{BaseUrl}/en/api-kontrol/", new Dictionary<string, string>
            {
                ["Accept"] = "application/json, text/html, */*;q=0.8",
                ["Referer"] = $"{BaseUrl}/",
                ["Cookie"] = cookie,
                ["User-Agent"] = Ua,
            }, 15);
            resp.EnsureSuccess();
            var data = Json.Parse(resp.Body);
            if (data is JsonArray arr) return ParseJsonEmails(arr, address);
            if (data is JsonObject obj)
            {
                foreach (var key in new[] { "mails", "emails", "messages", "data", "inbox" })
                    if (obj[key] is JsonArray items) return ParseJsonEmails(items, address);
                if (Json.Str(obj, "id").Length > 0 || Json.Str(obj, "subject").Length > 0 || Json.Str(obj, "from").Length > 0)
                    return ParseJsonEmails(new JsonArray(obj.DeepClone()), address);
            }
        }
        catch { /* 回退 HTML */ }

        return ParseHtmlEmails(cookie, address);
    }

    private static List<Email> ParseJsonEmails(JsonArray items, string recipient)
    {
        var emails = new List<Email>();
        foreach (var item in items)
        {
            if (item is not JsonObject raw) continue;
            var normalized = Json.ToDict(raw);
            if (normalized.ContainsKey("body") && !normalized.ContainsKey("html")) normalized["html"] = normalized["body"];
            if (normalized.ContainsKey("content") && !normalized.ContainsKey("html")) normalized["html"] = normalized["content"];
            emails.Add(Normalize.NormalizeEmail(normalized, recipient));
        }
        return emails;
    }

    private static List<Email> ParseHtmlEmails(string cookie, string recipient)
    {
        var resp = Http.Get(BaseUrl, new Dictionary<string, string>
        {
            ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            ["Referer"] = $"{BaseUrl}/",
            ["Cookie"] = cookie,
            ["User-Agent"] = Ua,
        }, 15);
        resp.EnsureSuccess();
        var html = resp.Body;

        var emails = new List<Email>();
        var mailItems = Regex.Matches(html,
            "<li[^>]*class=[\"'][^\"']*\\bmail\\b[^\"']*[\"'][^>]*>(.*?)</li>",
            RegexOptions.Singleline | RegexOptions.IgnoreCase);

        var idx = 0;
        foreach (Match mi in mailItems)
        {
            idx++;
            var itemHtml = mi.Groups[1].Value;
            var mailId = "";
            var subject = "";
            var fromAddr = "";
            var date = "";

            var linkMatch = Regex.Match(itemHtml, "href=[\"']/?([^\"']*?/?\\d+)[\"']", RegexOptions.IgnoreCase);
            if (linkMatch.Success)
            {
                var seg = linkMatch.Groups[1].Value.Trim().TrimEnd('/');
                var lastSlash = seg.LastIndexOf('/');
                mailId = lastSlash >= 0 ? seg[(lastSlash + 1)..] : seg;
            }
            if (mailId.Length == 0)
            {
                var dataIdMatch = Regex.Match(itemHtml, "data-id=[\"']([^\"']+)[\"']", RegexOptions.IgnoreCase);
                if (dataIdMatch.Success) mailId = dataIdMatch.Groups[1].Value.Trim();
            }
            if (mailId.Length == 0) mailId = idx.ToString();

            var subjectMatch = Regex.Match(itemHtml, "class=[\"'][^\"']*\\bsubject\\b[^\"']*[\"'][^>]*>(.*?)</",
                RegexOptions.Singleline | RegexOptions.IgnoreCase);
            if (subjectMatch.Success) subject = Regex.Replace(subjectMatch.Groups[1].Value, "<[^>]+>", "").Trim();

            var fromMatch = Regex.Match(itemHtml, "class=[\"'][^\"']*\\b(?:from|sender)\\b[^\"']*[\"'][^>]*>(.*?)</",
                RegexOptions.Singleline | RegexOptions.IgnoreCase);
            if (fromMatch.Success) fromAddr = Regex.Replace(fromMatch.Groups[1].Value, "<[^>]+>", "").Trim();

            var dateMatch = Regex.Match(itemHtml, "class=[\"'][^\"']*\\b(?:date|time)\\b[^\"']*[\"'][^>]*>(.*?)</",
                RegexOptions.Singleline | RegexOptions.IgnoreCase);
            if (dateMatch.Success) date = Regex.Replace(dateMatch.Groups[1].Value, "<[^>]+>", "").Trim();

            var bodyHtml = "";
            var bodyText = "";
            if (mailId.Length > 0 && IsAllDigits(mailId))
            {
                try
                {
                    var detail = FetchMailDetail(cookie, mailId);
                    bodyHtml = detail.GetValueOrDefault("html", "");
                    bodyText = detail.GetValueOrDefault("text", "");
                    if (subject.Length == 0 && detail.GetValueOrDefault("subject", "").Length > 0) subject = detail["subject"];
                    if (fromAddr.Length == 0 && detail.GetValueOrDefault("from", "").Length > 0) fromAddr = detail["from"];
                    if (date.Length == 0 && detail.GetValueOrDefault("date", "").Length > 0) date = detail["date"];
                }
                catch { /* 忽略 */ }
            }

            emails.Add(new Email
            {
                Id = mailId,
                From = fromAddr,
                To = recipient,
                Subject = subject,
                Text = bodyText,
                Html = bodyHtml,
                Date = date,
                IsRead = false,
                Attachments = new List<EmailAttachment>(),
            });
        }
        return emails;
    }

    private static bool IsAllDigits(string s)
    {
        if (s.Length == 0) return false;
        foreach (var c in s) if (!char.IsDigit(c)) return false;
        return true;
    }

    private static Dictionary<string, string> FetchMailDetail(string cookie, string mailId)
    {
        var resp = Http.Get($"{BaseUrl}/{mailId}", new Dictionary<string, string>
        {
            ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            ["Referer"] = $"{BaseUrl}/",
            ["Cookie"] = cookie,
            ["User-Agent"] = Ua,
        }, 15);
        resp.EnsureSuccess();
        var html = resp.Body;

        var result = new Dictionary<string, string> { ["html"] = "", ["text"] = "", ["subject"] = "", ["from"] = "", ["date"] = "" };

        var iframeMatch = Regex.Match(html, "<iframe[^>]+srcdoc=[\"']([^\"']+)[\"']", RegexOptions.Singleline | RegexOptions.IgnoreCase);
        if (iframeMatch.Success)
        {
            result["html"] = WebUtility.HtmlDecode(iframeMatch.Groups[1].Value);
        }
        else
        {
            var iframeSrcMatch = Regex.Match(html, "<iframe[^>]+src=[\"']([^\"']+)[\"']", RegexOptions.Singleline | RegexOptions.IgnoreCase);
            if (iframeSrcMatch.Success)
            {
                var src = iframeSrcMatch.Groups[1].Value;
                if (!src.StartsWith("http")) src = $"{BaseUrl}/{src.TrimStart('/')}";
                try
                {
                    var iframeResp = Http.Get(src, new Dictionary<string, string>
                    {
                        ["Accept"] = "text/html, */*",
                        ["Referer"] = $"{BaseUrl}/{mailId}",
                        ["Cookie"] = cookie,
                        ["User-Agent"] = Ua,
                    }, 15);
                    iframeResp.EnsureSuccess();
                    result["html"] = iframeResp.Body;
                }
                catch { /* 忽略 */ }
            }
        }

        if (result["html"].Length == 0)
        {
            var bodyMatch = Regex.Match(html,
                "class=[\"'][^\"']*\\b(?:mail-body|mail-content|message-body|content)\\b[^\"']*[\"'][^>]*>(.*?)</div>",
                RegexOptions.Singleline | RegexOptions.IgnoreCase);
            if (bodyMatch.Success) result["html"] = bodyMatch.Groups[1].Value.Trim();
        }

        if (result["html"].Length > 0) result["text"] = Regex.Replace(result["html"], "<[^>]+>", "").Trim();

        var subjMatch = Regex.Match(html, "class=[\"'][^\"']*\\bsubject\\b[^\"']*[\"'][^>]*>(.*?)</", RegexOptions.Singleline | RegexOptions.IgnoreCase);
        if (subjMatch.Success) result["subject"] = Regex.Replace(subjMatch.Groups[1].Value, "<[^>]+>", "").Trim();

        var fromMatch = Regex.Match(html, "class=[\"'][^\"']*\\b(?:from|sender)\\b[^\"']*[\"'][^>]*>(.*?)</", RegexOptions.Singleline | RegexOptions.IgnoreCase);
        if (fromMatch.Success) result["from"] = Regex.Replace(fromMatch.Groups[1].Value, "<[^>]+>", "").Trim();

        var dateMatch = Regex.Match(html, "class=[\"'][^\"']*\\b(?:date|time)\\b[^\"']*[\"'][^>]*>(.*?)</", RegexOptions.Singleline | RegexOptions.IgnoreCase);
        if (dateMatch.Success) result["date"] = Regex.Replace(dateMatch.Groups[1].Value, "<[^>]+>", "").Trim();

        return result;
    }
}
