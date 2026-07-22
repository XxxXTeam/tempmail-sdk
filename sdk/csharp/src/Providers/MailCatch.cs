using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// MailCatch 渠道 — https://mailcatch.com。
/// 公开收件箱，无需认证；创建随机用户名 + @mailcatch.com（token 存用户名）。
/// 收信：GET /api/list/{inbox} 取列表 HTML（正则提取 data-* 属性），
/// 再逐封 GET /api/data/{inbox}/{id} 取正文 HTML。
/// </summary>
public static class MailCatch
{
    private const string Channel = "mailcatch";
    private const string BaseUrl = "https://mailcatch.com";
    private const string DomainName = "mailcatch.com";

    private static readonly Regex ItemRe = new(
        "class=\"email-item\"\\s+data-email-id=\"([^\"]*)\"\\s+data-subject=\"([^\"]*)\"\\s+data-timestamp=\"([^\"]*)\"\\s+data-sender=\"([^\"]*)\"",
        RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    private static readonly Random Rand = new();

    private static string RandomLocal()
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder("sdk");
        for (var i = 0; i < 16; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string LocalPart(string email) => (email ?? "").Trim().Split('@')[0].Trim();

    /// <summary>创建临时邮箱（无需 API 调用）</summary>
    public static EmailInfo Generate()
    {
        var local = RandomLocal();
        return new EmailInfo(Channel, $"{local}@{DomainName}", local);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        if (string.IsNullOrEmpty(token)) throw new ArgumentException("mailcatch: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new ArgumentException("mailcatch: 邮箱地址不能为空");

        var inbox = token.Trim();
        if (inbox.Length == 0) inbox = LocalPart(addr);
        if (inbox.Length == 0) throw new ArgumentException("mailcatch: 无法确定收件箱名称");

        var resp = Http.Get($"{BaseUrl}/api/list/{inbox}", Headers, 15);
        resp.EnsureSuccess();

        var emails = new List<Email>();
        foreach (Match m in ItemRe.Matches(resp.Body))
        {
            var emailId = m.Groups[1].Value.Trim();
            if (emailId.Length == 0) continue;

            var bodyHtml = "";
            try
            {
                var detailResp = Http.Get($"{BaseUrl}/api/data/{inbox}/{emailId}", Headers, 15);
                if (detailResp.StatusCode == 200) bodyHtml = detailResp.Body.Trim();
            }
            catch { /* 忽略单封失败 */ }

            emails.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = emailId,
                ["from"] = m.Groups[4].Value.Trim(),
                ["to"] = addr,
                ["subject"] = m.Groups[2].Value.Trim(),
                ["html"] = bodyHtml,
                ["date"] = m.Groups[3].Value.Trim(),
            }, addr));
        }
        return emails;
    }
}
