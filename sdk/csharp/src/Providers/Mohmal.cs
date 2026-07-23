using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// mohmal.com 渠道 — https://www.mohmal.com。
/// 基于 HTML 解析，使用 connect.sid session cookie 维持会话。
/// 创建：GET /en/create/random（服务端下发 cookie 并重定向到 /en/inbox），从页面提取 data-email。
/// 收信：GET /en/inbox 解析表格行取邮件 ID，逐条 GET /en/message/{id} 取详情。
/// 说明：本端共享 HttpClient 自动跟随重定向且默认启用 Cookie 罐，session cookie 由罐维持；
/// token 中额外编码可见的 Set-Cookie 作为补充。
/// </summary>
public static class Mohmal
{
    private const string Channel = "mohmal";
    private const string Origin = "https://www.mohmal.com";
    private const string TokPrefix = "moh1:";

    private static readonly Regex DataEmailRe = new("data-email=\"([^\"]+)\"", RegexOptions.Compiled);
    private static readonly Regex MessageLinkRe = new("/en/message/(\\d+)", RegexOptions.Compiled);
    private static readonly Regex MessageBodyOpenRe = new(
        "<div[^>]*class=\"[^\"]*(?:mail-content|message-body)[^\"]*\"[^>]*>", RegexOptions.IgnoreCase | RegexOptions.Compiled);

    /// <summary>
    /// 使用栈式深度匹配提取 mail-content/message-body div 的完整内部 HTML，
    /// 避免非贪婪正则在嵌套 div 时截断正文。
    /// </summary>
    private static string ExtractBodyHtml(string page)
    {
        var m = MessageBodyOpenRe.Match(page);
        if (!m.Success) return "";
        int start = m.Index + m.Length;
        int pos = start;
        int depth = 1;
        while (pos < page.Length && depth > 0)
        {
            int nextOpen = page.IndexOf("<div", pos, StringComparison.OrdinalIgnoreCase);
            int nextClose = page.IndexOf("</div>", pos, StringComparison.OrdinalIgnoreCase);
            if (nextClose == -1) break;
            if (nextOpen != -1 && nextOpen < nextClose)
            {
                depth++;
                pos = nextOpen + 4;
            }
            else
            {
                depth--;
                if (depth == 0)
                {
                    return page.Substring(start, nextClose - start).Trim();
                }
                pos = nextClose + 6;
            }
        }
        return "";
    }
    private static readonly Regex DetailFromRe = new(
        "<span[^>]*class=\"[^\"]*from[^\"]*\"[^>]*>([\\s\\S]*?)</span>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex DetailSubjectRe = new(
        "<span[^>]*class=\"[^\"]*subject[^\"]*\"[^>]*>([\\s\\S]*?)</span>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex DetailDateRe = new(
        "<span[^>]*class=\"[^\"]*date[^\"]*\"[^>]*>([\\s\\S]*?)</span>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex FromAddrRe = new("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}", RegexOptions.Compiled);
    private static readonly Regex TagRe = new("<[^>]+>", RegexOptions.Compiled);

    private static readonly Dictionary<string, string> DefaultHeaders = new()
    {
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "en-US,en;q=0.9",
        ["Cache-Control"] = "no-cache",
        ["DNT"] = "1",
        ["Pragma"] = "no-cache",
        ["Upgrade-Insecure-Requests"] = "1",
    };

    private static string GetUa()
    {
        var custom = Config.Get().Headers?.GetValueOrDefault("User-Agent");
        return string.IsNullOrEmpty(custom)
            ? "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0"
            : custom;
    }

    private static Dictionary<string, string> PageHeaders(string referer, string cookie = "")
    {
        var h = new Dictionary<string, string>(DefaultHeaders) { ["User-Agent"] = GetUa(), ["Referer"] = referer };
        if (cookie.Length > 0) h["Cookie"] = cookie;
        return h;
    }

    private static string StripTags(string s) => WebUtility.HtmlDecode(TagRe.Replace(s, "")).Trim();

    private static string EncodeToken(string cookieHdr)
    {
        var raw = Encoding.UTF8.GetBytes(Json.Serialize(new Dictionary<string, object?> { ["c"] = cookieHdr }));
        return TokPrefix + Convert.ToBase64String(raw);
    }

    private static string DecodeToken(string tok)
    {
        if (!tok.StartsWith(TokPrefix)) return "";
        try
        {
            var data = Convert.FromBase64String(tok[TokPrefix.Length..]);
            var o = Json.Parse(Encoding.UTF8.GetString(data)) as JsonObject;
            return Json.Str(o, "c").Trim();
        }
        catch { return ""; }
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var createUrl = $"{Origin}/en/create/random";
        var inboxUrl = $"{Origin}/en/inbox";

        // 共享客户端自动跟随重定向；先请求 create/random（其 Set-Cookie 由 Cookie 罐维持）
        var r1 = Http.Get(createUrl, PageHeaders(Origin));
        var cookieHdr = ProviderHttpUtil.MergeCookies("", r1);

        // 再访问 inbox 页面获取邮箱地址（Cookie 罐已带 session）
        var r2 = Http.Get(inboxUrl, PageHeaders(createUrl, cookieHdr));
        r2.EnsureSuccess();
        cookieHdr = ProviderHttpUtil.MergeCookies(cookieHdr, r2);
        var pageHtml = r2.Body;

        var m = DataEmailRe.Match(pageHtml);
        if (!m.Success) throw new Exception("mohmal: unable to extract email address from inbox page");
        var email = WebUtility.HtmlDecode(m.Groups[1].Value).Trim();
        if (email.Length == 0 || !email.Contains('@')) throw new Exception($"mohmal: invalid email address: {email}");

        return new EmailInfo(Channel, email, EncodeToken(cookieHdr));
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        var cookieHdr = DecodeToken((token ?? "").Trim());
        var inboxUrl = $"{Origin}/en/inbox";

        var r = Http.Get(inboxUrl, PageHeaders(Origin, cookieHdr));
        r.EnsureSuccess();
        var inboxHtml = r.Body;

        var seen = new HashSet<string>();
        var uniqueIds = new List<string>();
        foreach (Match mm in MessageLinkRe.Matches(inboxHtml))
        {
            var mid = mm.Groups[1].Value;
            if (seen.Add(mid)) uniqueIds.Add(mid);
        }
        if (uniqueIds.Count == 0) return new List<Email>();

        var result = new List<Email>();
        foreach (var mid in uniqueIds)
        {
            var detailUrl = $"{Origin}/en/message/{mid}";
            var rd = Http.Get(detailUrl, PageHeaders(inboxUrl, cookieHdr));
            if (rd.StatusCode != 200) continue;
            result.Add(Normalize.NormalizeEmail(ParseMessageDetail(rd.Body, mid, email), email));
        }
        return result;
    }

    private static Dictionary<string, object?> ParseMessageDetail(string page, string mid, string recipient)
    {
        var fromAddr = "";
        var fm = DetailFromRe.Match(page);
        if (fm.Success)
        {
            var fromRaw = WebUtility.HtmlDecode(fm.Groups[1].Value);
            var em = FromAddrRe.Match(fromRaw);
            fromAddr = em.Success ? em.Value : StripTags(fromRaw);
        }

        var subject = "";
        var sm = DetailSubjectRe.Match(page);
        if (sm.Success) subject = StripTags(sm.Groups[1].Value);

        var date = "";
        var dm = DetailDateRe.Match(page);
        if (dm.Success) date = StripTags(dm.Groups[1].Value);

        var bodyHtml = ExtractBodyHtml(page);

        return new Dictionary<string, object?>
        {
            ["id"] = mid,
            ["from"] = fromAddr,
            ["to"] = recipient,
            ["subject"] = subject,
            ["date"] = date,
            ["html"] = bodyHtml,
        };
    }
}
