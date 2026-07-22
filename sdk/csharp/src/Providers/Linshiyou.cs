using System;
using System.Collections.Generic;
using System.Globalization;
using System.Net;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// linshiyou.com 渠道。
/// 创建邮箱：GET /api/user?user 拿 NEXUS_TOKEN cookie 与响应体邮箱，token 存 Cookie 串。
/// 收信：GET /api/mail?unseen=1 返回自定义分隔符切分的 HTML 分片，解析元信息与 srcdoc 正文。
/// </summary>
public static class Linshiyou
{
    private const string Channel = "linshiyou";
    private const string Origin = "https://linshiyou.com";

    private static readonly Dictionary<string, string> DefaultHeaders = new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        ["accept-language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["cache-control"] = "no-cache",
        ["dnt"] = "1",
        ["pragma"] = "no-cache",
        ["priority"] = "u=1, i",
        ["referer"] = $"{Origin}/",
        ["sec-ch-ua"] = "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        ["sec-ch-ua-mobile"] = "?0",
        ["sec-ch-ua-platform"] = "\"Windows\"",
        ["sec-fetch-dest"] = "empty",
        ["sec-fetch-mode"] = "cors",
        ["sec-fetch-site"] = "same-origin",
    };

    private static readonly Regex ListIdRe = new("id=\"tmail-email-list-([a-f0-9]+)\"", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex DivRe = new("<div class=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</div>", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex SrcdocRe = new("\\ssrcdoc=\"([^\"]*)\"", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex DownloadRe = new("href=\"(/api/download\\?id=[^\"]+)\"", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex TagsRe = new("<[^>]+>", RegexOptions.Compiled);

    private static string StripTags(string s)
    {
        var t = TagsRe.Replace(s, " ");
        return string.Join(" ", WebUtility.HtmlDecode(t).Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries));
    }

    private static string PickDiv(string fragment, string className)
    {
        foreach (Match m in DivRe.Matches(fragment))
            if (m.Groups[1].Value == className)
                return StripTags(m.Groups[2].Value).Trim();
        return "";
    }

    private static string ParseDate(string s)
    {
        s = (s ?? "").Trim();
        if (s.Length == 0) return "";
        foreach (var fmt in new[] { "yyyy-MM-dd HH:mm:ss", "yyyy-MM-ddTHH:mm:ss" })
        {
            if (DateTime.TryParseExact(s, fmt, CultureInfo.InvariantCulture, DateTimeStyles.None, out var dt))
                return new DateTimeOffset(dt, TimeSpan.Zero).ToString("yyyy-MM-ddTHH:mm:ssZ", CultureInfo.InvariantCulture);
        }
        return "";
    }

    private static string ExtractSrcdoc(string bodyPart)
    {
        var m = SrcdocRe.Match(bodyPart);
        return m.Success ? WebUtility.HtmlDecode(m.Groups[1].Value) : "";
    }

    private static List<Email> ParseSegments(string raw, string recipient)
    {
        raw = (raw ?? "").Trim();
        var outList = new List<Email>();
        if (raw.Length == 0) return outList;

        foreach (var segRaw in raw.Split("<-----TMAILNEXTMAIL----->"))
        {
            var seg = segRaw.Trim();
            if (seg.Length == 0) continue;
            var parts = seg.Split("<-----TMAILCHOPPER----->", 2);
            var listPart = parts[0];
            var bodyPart = parts.Length > 1 ? parts[1] : "";

            var mid = ListIdRe.Match(listPart);
            if (!mid.Success) continue;
            var midStr = mid.Groups[1].Value;

            var fromList = PickDiv(listPart, "name");
            var subjectList = PickDiv(listPart, "subject");
            var previewList = PickDiv(listPart, "body");

            var fromBody = PickDiv(bodyPart, "tmail-email-sender");
            var timeBody = PickDiv(bodyPart, "tmail-email-time");
            var titleBody = PickDiv(bodyPart, "tmail-email-title");
            var htmlBody = ExtractSrcdoc(bodyPart);

            var fromAddr = fromBody.Length > 0 ? fromBody : fromList;
            var subject = titleBody.Length > 0 ? titleBody : subjectList;
            var text = previewList.Length > 0 ? previewList : StripTags(htmlBody);

            var attachments = new List<EmailAttachment>();
            var dm = DownloadRe.Match(bodyPart);
            if (dm.Success)
                attachments.Add(new EmailAttachment { Filename = "", Url = $"{Origin}{dm.Groups[1].Value}" });

            outList.Add(new Email
            {
                Id = midStr,
                From = fromAddr,
                To = recipient,
                Subject = subject,
                Text = text,
                Html = htmlBody,
                Date = ParseDate(timeBody),
                IsRead = false,
                Attachments = attachments,
            });
        }
        return outList;
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{Origin}/api/user?user", DefaultHeaders);
        resp.EnsureSuccess();

        var nexus = ProviderHttpUtil.ExtractCookieValue(resp, "NEXUS_TOKEN");
        if (nexus.Length == 0) throw new Exception("linshiyou: NEXUS_TOKEN not in Set-Cookie");

        var email = (resp.Body ?? "").Trim();
        if (email.Length == 0 || !email.Contains('@')) throw new Exception("linshiyou: invalid email in response body");

        var token = $"NEXUS_TOKEN={nexus}; tmail-emails={email}";
        return new EmailInfo(Channel, email, token);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var headers = new Dictionary<string, string>(DefaultHeaders)
        {
            ["Cookie"] = token ?? "",
            ["x-requested-with"] = "XMLHttpRequest",
        };
        var resp = Http.Get($"{Origin}/api/mail?unseen=1", headers);
        resp.EnsureSuccess();
        return ParseSegments(resp.Body, email);
    }
}
