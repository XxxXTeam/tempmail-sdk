using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml.Linq;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Eyepaste 渠道 — https://eyepaste.com。
/// 无认证，直接生成随机用户名；收信通过 RSS XML（/inbox/{email}.rss）解析。
/// </summary>
public static class Eyepaste
{
    private const string Channel = "eyepaste";
    private const string DomainName = "eyepaste.com";
    private const string BaseUrl = "https://www.eyepaste.com";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/rss+xml, application/xml, text/xml, */*",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static readonly Random Rand = new();
    private static readonly Regex TagRe = new("<[^>]+>", RegexOptions.Compiled);

    private static string RandomUsername(int length = 10)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>生成随机邮箱地址，无需 API 调用</summary>
    public static EmailInfo Generate() => new(Channel, $"{RandomUsername()}@{DomainName}");

    private static Dictionary<string, string> ParseDescription(string desc)
    {
        var result = new Dictionary<string, string>
        {
            ["from"] = "", ["to"] = "", ["subject"] = "", ["date"] = "", ["body"] = "",
        };
        if (string.IsNullOrEmpty(desc)) return result;

        var pMatch = Regex.Match(desc, "<p[^>]*>(.*?)</p>", RegexOptions.Singleline | RegexOptions.IgnoreCase);
        if (pMatch.Success)
        {
            var meta = pMatch.Groups[1].Value.Trim();
            var fromM = Regex.Match(meta, "From:\\s*(.*?)(?:\\s+To:|$)", RegexOptions.Singleline);
            if (fromM.Success) result["from"] = fromM.Groups[1].Value.Trim();
            var toM = Regex.Match(meta, "To:\\s*(.*?)(?:\\s+Subject:|$)", RegexOptions.Singleline);
            if (toM.Success) result["to"] = toM.Groups[1].Value.Trim();
            var subM = Regex.Match(meta, "Subject:\\s*(.*?)(?:\\s+Date:|$)", RegexOptions.Singleline);
            if (subM.Success) result["subject"] = subM.Groups[1].Value.Trim();
            var dateM = Regex.Match(meta, "Date:\\s*(.*?)$", RegexOptions.Singleline);
            if (dateM.Success) result["date"] = dateM.Groups[1].Value.Trim();

            var pEnd = desc.IndexOf("</p>", StringComparison.OrdinalIgnoreCase);
            if (pEnd != -1)
            {
                var body = desc[(pEnd + 4)..].Trim();
                if (body.Length > 0) result["body"] = body;
            }
        }
        return result;
    }

    /// <summary>获取邮件列表（RSS XML 解析）</summary>
    public static List<Email> GetEmails(string email)
    {
        if (string.IsNullOrWhiteSpace(email)) throw new ArgumentException("eyepaste: empty email");
        var e = email.Trim();
        var resp = Http.Get($"{BaseUrl}/inbox/{e}.rss", Headers, 15);
        resp.EnsureSuccess();
        var content = resp.Body;
        var emails = new List<Email>();
        if (string.IsNullOrWhiteSpace(content)) return emails;

        XDocument doc;
        try { doc = XDocument.Parse(content); }
        catch { return emails; }

        var channelElem = doc.Root?.Element("channel");
        if (channelElem is null) return emails;

        var idx = 0;
        foreach (var item in channelElem.Elements("item"))
        {
            idx++;
            var title = (item.Element("title")?.Value ?? "").Trim();
            var desc = (item.Element("description")?.Value ?? "").Trim();

            var parsed = ParseDescription(desc);
            var subject = parsed["subject"].Length > 0 ? parsed["subject"] : title;
            var bodyHtml = parsed["body"];
            var text = bodyHtml.Length > 0 ? TagRe.Replace(bodyHtml, "").Trim() : "";

            emails.Add(new Email
            {
                Id = idx.ToString(),
                From = parsed["from"],
                To = parsed["to"].Length > 0 ? parsed["to"] : e,
                Subject = subject,
                Text = text,
                Html = bodyHtml,
                Date = parsed["date"],
                IsRead = false,
                Attachments = new List<EmailAttachment>(),
            });
        }
        return emails;
    }
}
