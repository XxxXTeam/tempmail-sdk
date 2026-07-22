using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Mailnesia 渠道 — https://mailnesia.com。
/// 公开收件箱，无需认证；创建随机用户名后访问 /mailbox/{local} 激活。
/// 收信：解析邮箱页面表格行，逐封拉取 /mailbox/{local}/{id} 详情提取 text/html。
/// </summary>
public static class Mailnesia
{
    private const string Channel = "mailnesia";
    private const string BaseUrl = "https://mailnesia.com";
    private const string DomainName = "mailnesia.com";

    private static readonly Dictionary<string, string> Headers = new() { ["Accept"] = "text/html,*/*" };

    private static readonly Regex RowRe = new(
        "<tr\\s+id=\"([^\"]+)\"[^>]*class=\"[^\"]*\\bemailheader\\b[^\"]*\"[^>]*>(.*?)</tr>",
        RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex TimeRe = new("<time\\s+datetime=\"([^\"]+)\"",
        RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex AnchorRe = new("<a\\b[^>]*class=\"email\"[^>]*>(.*?)</a>",
        RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex TagRe = new("<[^>]+>", RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex WsRe = new("\\s+", RegexOptions.Compiled);

    private static readonly Random Rand = new();

    private static string RandomLocal()
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder("sdk");
        for (var i = 0; i < 16; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string LocalPart(string email) => (email ?? "").Trim().Split('@')[0].Trim();

    private static string MailboxUrl(string local) => $"{BaseUrl}/mailbox/{WebUtility.UrlEncode(local)}";
    private static string DetailUrl(string local, string messageId) => $"{MailboxUrl(local)}/{WebUtility.UrlEncode(messageId)}";

    private static string FetchHtml(string url)
    {
        var resp = Http.Get(url, Headers, 15);
        resp.EnsureSuccess();
        return resp.Body;
    }

    private static string CleanText(string raw) =>
        WsRe.Replace(WebUtility.HtmlDecode(TagRe.Replace(raw ?? "", " ")), " ").Trim();

    private static List<Dictionary<string, object?>> ParseRows(string page)
    {
        var rows = new List<Dictionary<string, object?>>();
        foreach (Match match in RowRe.Matches(page))
        {
            var messageId = match.Groups[1].Value.Trim();
            var rowHtml = match.Groups[2].Value;
            var dateMatch = TimeRe.Match(rowHtml);
            var anchors = new List<string>();
            foreach (Match m in AnchorRe.Matches(rowHtml)) anchors.Add(CleanText(m.Groups[1].Value));
            if (anchors.Count < 3) continue;
            rows.Add(new Dictionary<string, object?>
            {
                ["id"] = messageId,
                ["date"] = dateMatch.Success ? WebUtility.HtmlDecode(dateMatch.Groups[1].Value.Trim()) : "",
                ["from"] = anchors[0],
                ["to"] = anchors[1],
                ["subject"] = anchors[2],
            });
        }
        return rows;
    }

    private static string ExtractDivById(string page, string divId, string nextId = "")
    {
        var needle = $"id=\"{divId}\"";
        var pos = page.IndexOf(needle, StringComparison.Ordinal);
        if (pos < 0) return "";
        var openEnd = page.IndexOf('>', pos);
        if (openEnd < 0) return "";
        var start = openEnd + 1;
        var end = -1;
        if (nextId.Length > 0)
            end = page.IndexOf($"<div id=\"{nextId}\"", start, StringComparison.Ordinal);
        if (end < 0)
        {
            var close = page.IndexOf("</div>", start, StringComparison.Ordinal);
            if (close >= 0) end = close + "</div>".Length;
        }
        if (end < 0) return "";
        var content = page[start..end].Trim();
        if (content.EndsWith("</div>")) content = content[..^"</div>".Length].Trim();
        return content;
    }

    private static string ExtractPlain(string page, string messageId)
    {
        var pattern = new Regex(
            $"<div\\s+id=\"text_plain_{Regex.Escape(messageId)}\"[^>]*>\\s*<pre>(.*?)</pre>\\s*</div>",
            RegexOptions.IgnoreCase | RegexOptions.Singleline);
        var match = pattern.Match(page);
        return match.Success ? WebUtility.HtmlDecode(match.Groups[1].Value).Trim() : "";
    }

    private static Dictionary<string, object?> FetchDetail(string local, Dictionary<string, object?> row)
    {
        var messageId = (row.GetValueOrDefault("id") as string ?? "").Trim();
        if (messageId.Length == 0) return row;
        var page = FetchHtml(DetailUrl(local, messageId));
        var detail = new Dictionary<string, object?>(row)
        {
            ["text"] = ExtractPlain(page, messageId),
            ["html"] = ExtractDivById(page, $"text_html_{messageId}", $"text_plain_{messageId}"),
        };
        return detail;
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var local = RandomLocal();
        FetchHtml(MailboxUrl(local));
        return new EmailInfo(Channel, $"{local}@{DomainName}");
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email)
    {
        var local = LocalPart(email);
        if (local.Length == 0) throw new ArgumentException("mailnesia: empty email");

        var page = FetchHtml(MailboxUrl(local));
        var rows = ParseRows(page);
        var emails = new List<Email>();
        foreach (var row in rows)
        {
            try { emails.Add(Normalize.NormalizeEmail(FetchDetail(local, row), email)); }
            catch { emails.Add(Normalize.NormalizeEmail(row, email)); }
        }
        return emails;
    }
}
