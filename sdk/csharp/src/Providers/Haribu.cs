using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Haribu 渠道 — https://haribu.net。
/// Tempail 类模式，需 session cookie 维持会话。
/// 创建：GET / 从 HTML 提取 &lt;input id="eposta_adres"&gt; 邮箱，cookie 编码进 token。
/// 收信：先 GET /en/api-kontrol/ 触发检查，再 GET / 解析 &lt;li class="mail"&gt; 条目。
/// </summary>
public static class Haribu
{
    private const string Channel = "haribu";
    private const string BaseUrl = "https://haribu.net";
    private const string TokPrefix = "haribu1:";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8",
        ["Cache-Control"] = "no-cache",
        ["DNT"] = "1",
        ["Pragma"] = "no-cache",
        ["Upgrade-Insecure-Requests"] = "1",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static readonly Regex EmailInputRe = new(
        "<input[^>]+id\\s*=\\s*[\"']eposta_adres[\"'][^>]+value\\s*=\\s*[\"']([^\"']+)[\"']", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex EmailInputRe2 = new(
        "<input[^>]+value\\s*=\\s*[\"']([^\"']+@[^\"']+)[\"'][^>]+id\\s*=\\s*[\"']eposta_adres[\"']", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex MailItemRe = new(
        "<li\\s+class\\s*=\\s*[\"']mail[\"'][^>]*>([\\s\\S]*?)</li>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex FromRe = new(
        "<span\\s+class\\s*=\\s*[\"']mail_gonderen[\"'][^>]*>([\\s\\S]*?)</span>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex SubjectRe = new(
        "<span\\s+class\\s*=\\s*[\"']mail_konu[\"'][^>]*>([\\s\\S]*?)</span>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex DateRe = new(
        "<span\\s+class\\s*=\\s*[\"']mail_zaman[\"'][^>]*>([\\s\\S]*?)</span>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex MailLinkRe = new(
        "href\\s*=\\s*[\"']([^\"']*(?:mail|read|view)[^\"']*)[\"']", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex BodyRe = new(
        "<div\\s+(?:id|class)\\s*=\\s*[\"'](?:mail_icerik|icerik|mail-content|message-body)[\"'][^>]*>([\\s\\S]*?)</div>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex TagRe = new("<[^>]+>", RegexOptions.Compiled);

    private static string StripTags(string s) => TagRe.Replace(s, " ").Trim();

    private static string EncodeSess(string cookieHdr)
    {
        var payload = Encoding.UTF8.GetBytes(Json.Serialize(new Dictionary<string, object?> { ["c"] = cookieHdr }));
        return TokPrefix + Convert.ToBase64String(payload);
    }

    private static string DecodeSess(string token)
    {
        if (!token.StartsWith(TokPrefix)) throw new ArgumentException("haribu: 无效的会话令牌");
        var raw = token[TokPrefix.Length..];
        string cookieHdr;
        try
        {
            var decoded = Convert.FromBase64String(raw);
            var data = Json.Parse(Encoding.UTF8.GetString(decoded)) as JsonObject;
            cookieHdr = Json.Str(data, "c");
        }
        catch { throw new ArgumentException("haribu: 无效的会话令牌"); }
        if (cookieHdr.Length == 0) throw new ArgumentException("haribu: 会话令牌中缺少 cookie");
        return cookieHdr;
    }

    private static string ExtractEmail(string html)
    {
        var m = EmailInputRe.Match(html);
        if (m.Success)
        {
            var addr = WebUtility.HtmlDecode(m.Groups[1].Value).Trim();
            if (addr.Length > 0 && addr.Contains('@')) return addr;
        }
        m = EmailInputRe2.Match(html);
        if (m.Success)
        {
            var addr = WebUtility.HtmlDecode(m.Groups[1].Value).Trim();
            if (addr.Length > 0 && addr.Contains('@')) return addr;
        }
        throw new Exception("haribu: 未找到邮箱地址（eposta_adres）");
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get(BaseUrl, Headers, 15);
        resp.EnsureSuccess();
        var html = resp.Body;
        if (string.IsNullOrEmpty(html)) throw new Exception("haribu: 首页响应为空");

        var email = ExtractEmail(html);
        var cookieHdr = ProviderHttpUtil.MergeCookies("", resp);
        var token = EncodeSess(cookieHdr);
        return new EmailInfo(Channel, email, token);
    }

    private static string FetchDetail(string detailUrl, string cookieHdr)
    {
        try
        {
            var h = new Dictionary<string, string>(Headers) { ["Cookie"] = cookieHdr, ["Referer"] = BaseUrl };
            var resp = Http.Get(detailUrl, h, 15);
            if (resp.StatusCode is < 200 or >= 300) return "";
            var m = BodyRe.Match(resp.Body ?? "");
            if (m.Success) return m.Groups[1].Value.Trim();
        }
        catch { /* 忽略 */ }
        return "";
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrWhiteSpace(email)) throw new ArgumentException("haribu: 邮箱地址为空");
        var e = email.Trim();
        var cookieHdr = DecodeSess(token ?? "");

        try
        {
            var kontrolHeaders = new Dictionary<string, string>(Headers)
            {
                ["Cookie"] = cookieHdr, ["Referer"] = BaseUrl, ["X-Requested-With"] = "XMLHttpRequest",
            };
            Http.Get($"{BaseUrl}/en/api-kontrol/", kontrolHeaders, 15);
        }
        catch { /* 忽略 */ }

        var inboxHeaders = new Dictionary<string, string>(Headers) { ["Cookie"] = cookieHdr, ["Referer"] = BaseUrl };
        var inboxResp = Http.Get(BaseUrl, inboxHeaders, 15);
        inboxResp.EnsureSuccess();
        var htmlStr = inboxResp.Body ?? "";

        var emails = new List<Email>();
        var idx = 0;
        foreach (Match item in MailItemRe.Matches(htmlStr))
        {
            var content = item.Groups[1].Value;
            var raw = new Dictionary<string, object?> { ["id"] = $"haribu-{idx}", ["to"] = e };

            var fm = FromRe.Match(content);
            if (fm.Success) raw["from"] = WebUtility.HtmlDecode(StripTags(fm.Groups[1].Value)).Trim();
            var sm = SubjectRe.Match(content);
            if (sm.Success) raw["subject"] = WebUtility.HtmlDecode(StripTags(sm.Groups[1].Value)).Trim();
            var dm = DateRe.Match(content);
            if (dm.Success) raw["date"] = WebUtility.HtmlDecode(StripTags(dm.Groups[1].Value)).Trim();

            var lm = MailLinkRe.Match(content);
            if (lm.Success)
            {
                var detailUrl = lm.Groups[1].Value;
                if (!detailUrl.StartsWith("http")) detailUrl = BaseUrl + "/" + detailUrl.TrimStart('/');
                var htmlBody = FetchDetail(detailUrl, cookieHdr);
                if (htmlBody.Length > 0) raw["html"] = htmlBody;
            }

            emails.Add(Normalize.NormalizeEmail(raw, e));
            idx++;
        }
        return emails;
    }
}
