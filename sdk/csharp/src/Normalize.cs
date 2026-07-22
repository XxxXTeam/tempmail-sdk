using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail;

/// <summary>
/// 邮件数据标准化模块。
/// 不同渠道 API 返回字段名各异，本模块通过多候选字段策略将其统一映射为标准 Email 结构。
/// provider 先把渠道原始字段塞进 Dictionary（键沿用渠道原名），再交由本模块归一。
/// </summary>
public static class Normalize
{
    /// <summary>将渠道原始字段字典标准化为统一 Email 结构</summary>
    public static Email NormalizeEmail(IDictionary<string, object?> raw, string recipientEmail = "")
    {
        var text = NormalizeText(raw);
        var html = NormalizeHtml(raw);

        // 修正 text/html 错配
        if (!string.IsNullOrEmpty(text) && string.IsNullOrEmpty(html) && IsHtmlContent(text))
        {
            html = text;
            text = "";
        }
        if (string.IsNullOrEmpty(text) && !string.IsNullOrEmpty(html))
            text = HtmlToText(html);
        if (!string.IsNullOrEmpty(text) && string.IsNullOrEmpty(html))
            html = TextToHtml(text);

        return new Email
        {
            Id = GetStr(raw, "id", "eid", "_id", "mailboxId", "messageId", "mail_id"),
            From = GetStr(raw, "from_addr", "from_address", "fromAddress", "mail_sender",
                "sender", "address_from", "from_email", "from", "messageFrom"),
            To = NormalizeTo(raw, recipientEmail),
            Subject = GetStr(raw, "subject", "e_subject", "mail_title"),
            Text = text,
            Html = html,
            Date = NormalizeDate(raw),
            IsRead = NormalizeIsRead(raw),
            Attachments = NormalizeAttachments(raw.TryGetValue("attachments", out var a) ? a : null),
        };
    }

    private static string GetStr(IDictionary<string, object?> raw, params string[] keys)
    {
        foreach (var key in keys)
        {
            if (raw.TryGetValue(key, out var val) && val is not null)
            {
                var s = ValueToString(val);
                if (s is not null) return s;
            }
        }
        return "";
    }

    private static string? ValueToString(object val)
    {
        return val switch
        {
            string s => s,
            bool b => b ? "true" : "false",
            _ => Convert.ToString(val, CultureInfo.InvariantCulture),
        };
    }

    private static string NormalizeTo(IDictionary<string, object?> raw, string recipientEmail)
    {
        var result = GetStr(raw, "to", "to_address", "toAddress", "name_to", "email_address", "address");
        return string.IsNullOrEmpty(result) ? recipientEmail : result;
    }

    private static string NormalizeText(IDictionary<string, object?> raw) =>
        GetStr(raw, "text", "text_body", "preview_text", "mail_body_text", "body",
            "content", "body_text", "text_content", "description");

    private static string NormalizeHtml(IDictionary<string, object?> raw) =>
        GetStr(raw, "html", "html_body", "html_content", "body_html", "mail_body_html");

    // 日期候选键：static readonly 避免每次归一化都分配临时数组
    private static readonly string[] DateKeys = { "received_at", "receivedAt", "created_at", "createdAt", "date" };
    private static readonly string[] NumericDateKeys = { "timestamp", "e_date" };
    private static readonly string[] BoolReadKeys = { "seen", "read", "isRead" };
    private static readonly string[] IntReadKeys = { "is_read", "is_seen" };

    private static string NormalizeDate(IDictionary<string, object?> raw)
    {
        foreach (var key in DateKeys)
        {
            if (!raw.TryGetValue(key, out var val) || val is null) continue;
            if (val is string sv && sv.Length > 0)
            {
                var normalized = sv.Replace("Z", "+00:00");
                if (DateTimeOffset.TryParse(normalized, CultureInfo.InvariantCulture,
                        DateTimeStyles.RoundtripKind, out var dto))
                    return dto.ToString("o", CultureInfo.InvariantCulture);
                if (DateTime.TryParseExact(sv, "yyyy-MM-dd HH:mm:ss", CultureInfo.InvariantCulture,
                        DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal, out var dt))
                    return new DateTimeOffset(dt, TimeSpan.Zero).ToString("o", CultureInfo.InvariantCulture);
                return sv;
            }
            if (TryToDouble(val, out var num) && num > 0)
            {
                var iso = FromUnix(num);
                if (iso is not null) return iso;
            }
        }

        foreach (var key in NumericDateKeys)
        {
            if (!raw.TryGetValue(key, out var val) || val is null) continue;
            if (TryToDouble(val, out var num) && num > 0)
            {
                if (key == "timestamp" && num < 1e12)
                    return FromUnix(num) ?? "";
                return FromUnixMillis(num) ?? "";
            }
        }

        return "";
    }

    private static string? FromUnix(double val)
    {
        try
        {
            return val > 1e12
                ? DateTimeOffset.FromUnixTimeMilliseconds((long)val).ToString("o", CultureInfo.InvariantCulture)
                : DateTimeOffset.FromUnixTimeSeconds((long)val).ToString("o", CultureInfo.InvariantCulture);
        }
        catch { return null; }
    }

    private static string? FromUnixMillis(double val)
    {
        try { return DateTimeOffset.FromUnixTimeMilliseconds((long)val).ToString("o", CultureInfo.InvariantCulture); }
        catch { return null; }
    }

    private static bool NormalizeIsRead(IDictionary<string, object?> raw)
    {
        foreach (var key in BoolReadKeys)
            if (raw.TryGetValue(key, out var val) && val is bool b)
                return b;

        foreach (var key in IntReadKeys)
        {
            if (!raw.TryGetValue(key, out var val) || val is null) continue;
            switch (val)
            {
                case bool b2: return b2;
                case string s: return s == "1";
                default:
                    if (TryToDouble(val, out var n)) return (int)n != 0;
                    break;
            }
        }
        return false;
    }

    private static List<EmailAttachment> NormalizeAttachments(object? attachments)
    {
        var result = new List<EmailAttachment>();
        if (attachments is not IEnumerable<object?> list) return result;
        foreach (var item in list)
        {
            if (item is not IDictionary<string, object?> a) continue;
            result.Add(new EmailAttachment
            {
                Filename = FirstStr(a, "filename", "name") ?? "",
                Size = FirstLong(a, "size", "filesize"),
                ContentType = FirstStr(a, "contentType", "content_type", "mimeType", "mime_type"),
                Url = FirstStr(a, "url", "download_url", "downloadUrl"),
            });
        }
        return result;
    }

    private static string? FirstStr(IDictionary<string, object?> d, params string[] keys)
    {
        foreach (var k in keys)
            if (d.TryGetValue(k, out var v) && v is not null)
                return ValueToString(v);
        return null;
    }

    private static long? FirstLong(IDictionary<string, object?> d, params string[] keys)
    {
        foreach (var k in keys)
            if (d.TryGetValue(k, out var v) && v is not null && TryToDouble(v, out var n))
                return (long)n;
        return null;
    }

    private static bool TryToDouble(object val, out double result)
    {
        switch (val)
        {
            case double d: result = d; return true;
            case float f: result = f; return true;
            case long l: result = l; return true;
            case int i: result = i; return true;
            case string s when double.TryParse(s, NumberStyles.Any, CultureInfo.InvariantCulture, out var sr):
                result = sr; return true;
        }
        result = 0;
        return false;
    }

    /// <summary>检测内容是否为 HTML（只取前 200 字符）</summary>
    public static bool IsHtmlContent(string content)
    {
        var prefix = (content.Length > 200 ? content[..200] : content).Trim().ToLowerInvariant();
        if (prefix.StartsWith("<!doctype html") || prefix.StartsWith("<html") || prefix.StartsWith("<body"))
            return true;
        var trimmed = content.Trim().ToLowerInvariant();
        if (trimmed.Contains("<div") && trimmed.Contains("</div>")) return true;
        if (trimmed.Contains("<table") && trimmed.Contains("</table>")) return true;
        if (trimmed.Contains("<p") && trimmed.Contains("</p>") && trimmed.Contains("<")) return true;
        return false;
    }

    private static readonly Regex _tagRe = new("<[^>]+>", RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex _wsRe = new("\\s+", RegexOptions.Compiled);

    /// <summary>粗略 HTML 转纯文本：去标签、反转义、压缩空白</summary>
    public static string HtmlToText(string html)
    {
        if (string.IsNullOrEmpty(html)) return "";
        var noTags = _tagRe.Replace(html, " ");
        var unescaped = System.Net.WebUtility.HtmlDecode(noTags);
        return _wsRe.Replace(unescaped, " ").Trim();
    }

    private static string TextToHtml(string text) =>
        $"<html><body><pre>{System.Net.WebUtility.HtmlEncode(text)}</pre></body></html>";
}
