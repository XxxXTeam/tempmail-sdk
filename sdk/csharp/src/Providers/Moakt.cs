using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// moakt.com 及其全部镜像域名渠道实现（drmail.in / teml.net / tmpeml.com / tmpbox.net /
/// moakt.cc / disbox.net / tmpmail.org / tmpmail.net / tmails.net / disbox.org /
/// moakt.co / moakt.ws / tmail.ws / bareed.ws）。
///
/// 流程：GET 语言首页与收件箱 HTML；凭证为 tm_session 等 Cookie（序列化在 token 内）；
/// 列表解析 /{locale}/email/{uuid}，正文 GET .../html 解析 .email-body。
/// 需手动管理 Cookie 头，故使用禁用 Cookie 容器的裸客户端；创建邮箱时不跟随 302
/// 以捕获重定向响应中的 tm_session cookie。
/// </summary>
public static class Moakt
{
    private const string Channel = "moakt";
    private const string Origin = "https://www.moakt.com";
    private const string TokPrefix = "mok1:";

    private const string DefaultUa =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) " +
        "Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static readonly Regex EmailDivRe =
        new(@"<div\s+id=""email-address""\s*>([^<]+)</div>", RegexOptions.IgnoreCase | RegexOptions.Singleline);
    private static readonly Regex DomainOptionRe =
        new(@"<option\s+value=""([^""]+)"">\s*@[^<]+</option>", RegexOptions.IgnoreCase | RegexOptions.Singleline);
    private static readonly Regex MailDomainRe =
        new(@"^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?)+$", RegexOptions.IgnoreCase);
    private static readonly Regex HrefEmailRe =
        new(@"href=""(/[^""]+/email/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})""", RegexOptions.Compiled);
    private static readonly Regex TitleRe =
        new(@"<li\s+class=""title""\s*>([^<]*)</li>", RegexOptions.IgnoreCase | RegexOptions.Singleline);
    private static readonly Regex DateRe =
        new(@"<li\s+class=""date""[^>]*>[\s\S]*?<span[^>]*>([^<]+)</span>", RegexOptions.IgnoreCase | RegexOptions.Singleline);
    private static readonly Regex SenderRe =
        new(@"<li\s+class=""sender""[^>]*>[\s\S]*?<span[^>]*>([\s\S]*?)</span>\s*</li>", RegexOptions.IgnoreCase | RegexOptions.Singleline);
    private static readonly Regex BodyOpenRe =
        new(@"<div\s+class=""email-body""\s*>", RegexOptions.IgnoreCase);

    /// <summary>
    /// 使用栈式深度匹配提取 email-body div 的完整内部 HTML，
    /// 避免非贪婪正则在嵌套 div 时截断正文。
    /// </summary>
    private static string ExtractBodyHtml(string page)
    {
        var m = BodyOpenRe.Match(page);
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
    private static readonly Regex FromAddrRe =
        new(@"<([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})>", RegexOptions.Compiled);
    private static readonly Regex TagRe = new("<[^>]+>", RegexOptions.Compiled);

    private static readonly char[] LocalChars = "abcdefghijklmnopqrstuvwxyz0123456789".ToCharArray();
    private static readonly Random Rand = new();

    private static readonly Dictionary<string, string> DefaultHeaders = new()
    {
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["Cache-Control"] = "no-cache",
        ["DNT"] = "1",
        ["Pragma"] = "no-cache",
        ["Upgrade-Insecure-Requests"] = "1",
    };

    /// <summary>解析 domain 参数为 (locale, mailDomain) 二元组，逻辑对齐 Python _request_parts</summary>
    private static (string locale, string mailDomain) RequestParts(string? domain)
    {
        var s = (domain ?? "").Trim();
        if (s.Length == 0 || s.IndexOfAny(new[] { '/', '?', '#', '\\' }) >= 0)
            return ("zh", "");
        if (MailDomainRe.IsMatch(s))
            return ("zh", s.TrimStart('@').ToLowerInvariant());
        return (s, "");
    }

    /// <summary>解析首页可选域名集合</summary>
    private static HashSet<string> ParseServerDomains(string page)
    {
        var set = new HashSet<string>(StringComparer.Ordinal);
        foreach (Match m in DomainOptionRe.Matches(page))
        {
            var v = m.Groups[1].Value.Trim().TrimStart('@').ToLowerInvariant();
            if (v.Length > 0) set.Add(v);
        }
        return set;
    }

    private static string RandomLocal(int length = 12)
    {
        var sb = new StringBuilder(length);
        for (var i = 0; i < length; i++)
            sb.Append(LocalChars[Rand.Next(LocalChars.Length)]);
        return sb.ToString();
    }

    private static string EmailDomain(string email)
    {
        var idx = email.LastIndexOf('@');
        return idx < 0 ? "" : email[(idx + 1)..].Trim().ToLowerInvariant();
    }

    private static Dictionary<string, string> PageHeaders(string referer, string ua)
    {
        var h = new Dictionary<string, string>(DefaultHeaders)
        {
            ["User-Agent"] = ua,
            ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
            ["Referer"] = referer,
        };
        return h;
    }

    private static string CurrentUa()
    {
        var headers = Config.Get().Headers;
        if (headers is not null && headers.TryGetValue("User-Agent", out var ua) && !string.IsNullOrEmpty(ua))
            return ua;
        return DefaultUa;
    }

    /// <summary>将 locale 与 cookie 头编码为 mok1: 前缀的 base64 token</summary>
    private static string EncodeSess(string locale, string cookieHdr)
    {
        var raw = Json.Serialize(new Dictionary<string, object?> { ["l"] = locale, ["c"] = cookieHdr });
        return TokPrefix + Convert.ToBase64String(Encoding.UTF8.GetBytes(raw));
    }

    /// <summary>解码 token 为 (locale, cookie头)</summary>
    private static (string locale, string cookieHdr) DecodeSess(string tok)
    {
        if (!tok.StartsWith(TokPrefix, StringComparison.Ordinal))
            throw new Exception("moakt: invalid session token");
        JsonObject obj;
        try
        {
            var data = Convert.FromBase64String(tok[TokPrefix.Length..]);
            obj = Json.Parse(Encoding.UTF8.GetString(data)) as JsonObject
                  ?? throw new Exception("moakt: invalid session token");
        }
        catch (Exception)
        {
            throw new Exception("moakt: invalid session token");
        }
        var loc = Json.Str(obj, "l").Trim();
        var c = Json.Str(obj, "c").Trim();
        if (loc.Length == 0 || c.Length == 0)
            throw new Exception("moakt: invalid session token");
        return (loc, c);
    }

    private static string ParseInboxEmail(string html)
    {
        var m = EmailDivRe.Match(html);
        if (!m.Success) throw new Exception("moakt: email-address not found");
        var addr = WebUtility.HtmlDecode(m.Groups[1].Value.Trim());
        if (string.IsNullOrEmpty(addr)) throw new Exception("moakt: empty email-address");
        return addr;
    }

    private static string StripTags(string s) => TagRe.Replace(s, " ").Trim();

    /// <summary>从收件箱页解析邮件 UUID 列表（去重、跳过 /delete 链接）</summary>
    private static List<string> ListMailIds(string html)
    {
        var seen = new HashSet<string>(StringComparer.Ordinal);
        var outIds = new List<string>();
        foreach (Match m in HrefEmailRe.Matches(html))
        {
            var path = m.Groups[1].Value;
            if (path.Contains("/delete")) continue;
            var mid = path[(path.LastIndexOf('/') + 1)..];
            if (mid.Length == 36 && seen.Add(mid))
                outIds.Add(mid);
        }
        return outIds;
    }

    /// <summary>解析邮件详情页为原始字段字典</summary>
    private static Dictionary<string, object?> ParseDetail(string page, string mid, string recipient)
    {
        var from = "";
        var sm = SenderRe.Match(page);
        if (sm.Success)
        {
            var inner = WebUtility.HtmlDecode(sm.Groups[1].Value);
            from = StripTags(inner);
            var em = FromAddrRe.Match(inner);
            if (em.Success) from = em.Groups[1].Value.Trim();
        }
        var subj = "";
        var tm = TitleRe.Match(page);
        if (tm.Success) subj = WebUtility.HtmlDecode(tm.Groups[1].Value.Trim());
        var date = "";
        var dm = DateRe.Match(page);
        if (dm.Success) date = WebUtility.HtmlDecode(dm.Groups[1].Value.Trim());
        var body = ExtractBodyHtml(page);

        return new Dictionary<string, object?>
        {
            ["id"] = mid,
            ["to"] = recipient,
            ["from"] = from,
            ["subject"] = subj,
            ["date"] = date,
            ["html"] = body,
        };
    }

    /// <summary>创建 moakt 临时邮箱（可指定镜像域名，如 "drmail.in"；channel 为对外暴露的渠道标识）</summary>
    public static EmailInfo Generate(string? domain, string channel = Channel)
    {
        var (loc, mailDomain) = RequestParts(domain);
        var baseUrl = $"{Origin}/{loc}";
        var inbox = $"{baseUrl}/inbox";
        var ua = CurrentUa();

        var r1 = Http.RawGet(baseUrl, PageHeaders(baseUrl, ua));
        r1.EnsureSuccess();
        var cookieHdr = ProviderHttpUtil.MergeCookies("", r1);

        string postData;
        if (mailDomain.Length > 0)
        {
            if (!ParseServerDomains(r1.Body).Contains(mailDomain))
                throw new Exception($"moakt: unsupported domain {mailDomain}");
            postData = $"setemail=&username={WebUtility.UrlEncode(RandomLocal())}" +
                       $"&domain={WebUtility.UrlEncode(mailDomain)}&preferred_domain=";
        }
        else
        {
            postData = "random=1";
        }

        // POST /inbox 创建邮箱，不跟随 302 以捕获重定向响应中的 tm_session cookie
        var postHeaders = PageHeaders(baseUrl, ua);
        postHeaders["Cookie"] = cookieHdr;
        var r2 = Http.RawPost(inbox, postData, "application/x-www-form-urlencoded", postHeaders, followRedirect: false);
        cookieHdr = ProviderHttpUtil.MergeCookies(cookieHdr, r2);

        if (!ProviderHttpUtil.ParseCookieMap(cookieHdr).ContainsKey("tm_session"))
            throw new Exception("moakt: missing tm_session cookie");

        // GET /inbox 获取邮箱地址
        var getHeaders = PageHeaders(baseUrl, ua);
        getHeaders["Cookie"] = cookieHdr;
        var r3 = Http.RawGet(inbox, getHeaders);
        r3.EnsureSuccess();
        cookieHdr = ProviderHttpUtil.MergeCookies(cookieHdr, r3);

        var email = ParseInboxEmail(r3.Body);
        if (mailDomain.Length > 0 && EmailDomain(email) != mailDomain)
            throw new Exception($"moakt: domain mismatch expected={mailDomain} actual={EmailDomain(email)}");

        var tok = EncodeSess(loc, cookieHdr);
        return new EmailInfo(channel, email, tok);
    }

    /// <summary>获取 moakt 邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("moakt: token 不能为空");
        var (loc, cookieHdr) = DecodeSess(token);
        var inbox = $"{Origin}/{loc}/inbox";
        var baseRef = $"{Origin}/{loc}";
        var ua = CurrentUa();

        var listHeaders = PageHeaders(baseRef, ua);
        listHeaders["Cookie"] = cookieHdr;
        var r = Http.RawGet(inbox, listHeaders);
        r.EnsureSuccess();

        var ids = ListMailIds(r.Body);
        var result = new List<Email>();
        foreach (var mid in ids)
        {
            var detail = $"{Origin}/{loc}/email/{mid}/html";
            var refer = $"{Origin}/{loc}/email/{mid}";
            var detailHeaders = PageHeaders(refer, ua);
            detailHeaders["Cookie"] = cookieHdr;
            HttpResult rd;
            try { rd = Http.RawGet(detail, detailHeaders); }
            catch { continue; }
            if (rd.StatusCode != 200) continue;
            result.Add(Normalize.NormalizeEmail(ParseDetail(rd.Body, mid, email), email));
        }
        return result;
    }
}
