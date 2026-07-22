using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// anonbox.net (CCC) 渠道。
/// 创建邮箱：GET /en/ 解析 HTML，提取邮箱地址与 inbox/secret 令牌。
/// 收信：GET /{inbox}/{secret}/ 返回 mbox 文本，按 "From " 行切分并解析每封邮件。
/// </summary>
public static class AnonBox
{
    private const string Channel = "anonbox";
    private const string PageUrl = "https://anonbox.net/en/";
    private const string BaseUrl = "https://anonbox.net";

    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

    private static readonly Regex MailLinkRe = new(
        "<a href=\"https://anonbox\\.net/([a-z0-9-]+)/([A-Za-z0-9._~-]+)\">https://anonbox\\.net/[^\"]+</a>",
        RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex DdRe = new("<dd([^>]*)>([\\s\\S]*?)</dd>",
        RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex DisplayNoneRe = new("display\\s*:\\s*none", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex PRe = new("<p>([\\s\\S]*?)</p>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex SpanRe = new("<span\\b[^>]*>[\\s\\S]*?</span>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex TagRe = new("<[^>]+>", RegexOptions.Compiled);
    private static readonly Regex ExpiresRe = new("Your mail address is valid until:</dt>\\s*<dd><p>([^<]+)</p>",
        RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex MboxSplitRe = new("\\r?\\n(?=From )", RegexOptions.Compiled);

    private static string StripTags(string html)
    {
        var s = TagRe.Replace(html, "");
        return s.Replace("&nbsp;", " ").Replace("&amp;", "&").Replace("&lt;", "<").Replace("&gt;", ">").Trim();
    }

    /// <summary>与 Python _simple_hash 对齐的 31 进制哈希（base36 输出）</summary>
    private static string SimpleHash(string s)
    {
        uint h = 0;
        foreach (var c in s) h = (uint)((h * 31 + c) & 0xFFFFFFFF);
        if (h == 0) return "0";
        const string digits = "0123456789abcdefghijklmnopqrstuvwxyz";
        var sb = new StringBuilder();
        var n = h;
        while (n > 0) { sb.Insert(0, digits[(int)(n % 36)]); n /= 36; }
        return sb.ToString();
    }

    private static (string email, string token, string? exp) ParseEnPage(string html)
    {
        var m = MailLinkRe.Match(html);
        if (!m.Success) throw new Exception("anonbox: mailbox link not found");
        var inbox = m.Groups[1].Value;
        var secret = m.Groups[2].Value;
        var token = $"{inbox}/{secret}";

        string? addressHtml = null;
        foreach (Match dd in DdRe.Matches(html))
        {
            if (DisplayNoneRe.IsMatch(dd.Groups[1].Value)) continue;
            var pm = PRe.Match(dd.Groups[2].Value);
            if (!pm.Success) continue;
            var pInner = SpanRe.Replace(pm.Groups[1].Value, "");
            var display = StripTags(pInner);
            if (!display.Contains('@')) continue;
            var at2 = display.LastIndexOf('@');
            var domain = display[(at2 + 1)..].Trim().ToLowerInvariant();
            if (domain == "googlemail.com") continue;
            var expectedDomain = $"{inbox}.anonbox.net".ToLowerInvariant();
            if (domain != expectedDomain) continue;
            var local2 = display[..at2].Trim();
            if (local2.Length == 0) continue;
            addressHtml = display;
            break;
        }
        if (addressHtml is null) throw new Exception("anonbox: address paragraph not found");

        var at = addressHtml.IndexOf('@');
        if (at < 0) throw new Exception("anonbox: bad address");
        var local = addressHtml[..at].Trim();
        if (local.Length == 0) throw new Exception("anonbox: empty local part");
        var email = $"{local}@{inbox}.anonbox.net";

        var em = ExpiresRe.Match(html);
        var exp = em.Success ? em.Groups[1].Value.Trim() : null;
        return (email, token, exp);
    }

    private static string DecodeQpIfNeeded(string body, string headerBlock)
    {
        var te = Regex.Match(headerBlock, "^content-transfer-encoding:\\s*([^\\s]+)",
            RegexOptions.IgnoreCase | RegexOptions.Multiline);
        var enc = te.Success ? te.Groups[1].Value.ToLowerInvariant().Trim() : "";
        if (enc != "quoted-printable") return body.TrimEnd('\r', '\n');

        var soft = Regex.Replace(body, "=\\r?\\n", "");
        var b = Encoding.Latin1.GetBytes(soft);
        var outBytes = new List<byte>();
        var i = 0;
        while (i < b.Length)
        {
            if (b[i] == (byte)'=' && i + 2 < b.Length)
            {
                var hex = Encoding.ASCII.GetString(b, i + 1, 2);
                if (int.TryParse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var val))
                {
                    outBytes.Add((byte)val);
                    i += 3;
                    continue;
                }
            }
            outBytes.Add(b[i]);
            i++;
        }
        return Encoding.UTF8.GetString(outBytes.ToArray()).TrimEnd('\r', '\n');
    }

    private static Dictionary<string, object?> MboxBlockToRaw(string block, string recipient)
    {
        var normalized = block.Replace("\r\n", "\n");
        var lines = normalized.Split('\n');
        var i = 0;
        if (lines.Length > 0 && lines[0].StartsWith("From ")) i = 1;

        var headers = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var curKey = "";
        while (i < lines.Length)
        {
            var line = lines[i];
            if (line == "") { i++; break; }
            if ((line[0] == ' ' || line[0] == '\t') && curKey.Length > 0)
            {
                headers[curKey] = headers[curKey] + " " + line.Trim();
            }
            else
            {
                var idx = line.IndexOf(':');
                if (idx > 0)
                {
                    curKey = line[..idx].Trim().ToLowerInvariant();
                    headers[curKey] = line[(idx + 1)..].Trim();
                }
            }
            i++;
        }
        var body = string.Join("\n", lines[i..]);

        var ct = (headers.GetValueOrDefault("content-type") ?? "").ToLowerInvariant();
        var text = "";
        var html = "";
        if (ct.Contains("multipart/"))
        {
            var bm = Regex.Match(headers.GetValueOrDefault("content-type") ?? "",
                "boundary=\"?([^\";\\s]+)\"?", RegexOptions.IgnoreCase);
            if (bm.Success)
            {
                var boundary = Regex.Escape(bm.Groups[1].Value);
                var partRe = new Regex($"\\r?\\n--{boundary}(?:--)?\\r?\\n");
                foreach (var partRaw in partRe.Split(body))
                {
                    var part = partRaw.Trim();
                    if (part.Length == 0 || part == "--") continue;
                    var sep = part.IndexOf("\n\n", StringComparison.Ordinal);
                    if (sep < 0) continue;
                    var ph = part[..sep];
                    var pb = part[(sep + 2)..];
                    var pctM = Regex.Match(ph, "^content-type:\\s*([^\\s;]+)", RegexOptions.IgnoreCase | RegexOptions.Multiline);
                    var pct = pctM.Success ? pctM.Groups[1].Value.ToLowerInvariant() : "";
                    if (pct == "text/plain") text = DecodeQpIfNeeded(pb, ph);
                    else if (pct == "text/html") html = DecodeQpIfNeeded(pb, ph);
                }
            }
        }
        if (text.Length == 0 && html.Length == 0)
        {
            if (ct.Contains("text/html")) html = DecodeQpIfNeeded(body, headers.GetValueOrDefault("content-type") ?? "");
            else text = DecodeQpIfNeeded(body, headers.GetValueOrDefault("content-type") ?? "");
        }

        var dateStr = headers.GetValueOrDefault("date") ?? "";
        if (dateStr.Length > 0 && DateTimeOffset.TryParse(dateStr.Trim(), CultureInfo.InvariantCulture,
                DateTimeStyles.RoundtripKind, out var dto))
            dateStr = dto.ToString("o", CultureInfo.InvariantCulture);
        if (dateStr.Length == 0)
            dateStr = DateTimeOffset.UtcNow.ToString("o", CultureInfo.InvariantCulture);

        var msgId = headers.GetValueOrDefault("message-id");
        if (string.IsNullOrEmpty(msgId))
            msgId = SimpleHash(block.Length > 512 ? block[..512] : block);

        return new Dictionary<string, object?>
        {
            ["id"] = msgId,
            ["from"] = headers.GetValueOrDefault("from") ?? "",
            ["to"] = string.IsNullOrEmpty(headers.GetValueOrDefault("to")) ? recipient : headers["to"],
            ["subject"] = headers.GetValueOrDefault("subject") ?? "",
            ["body_text"] = text,
            ["body_html"] = html,
            ["date"] = dateStr,
            ["isRead"] = false,
            ["attachments"] = new List<object?>(),
        };
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get(PageUrl, new Dictionary<string, string>
        {
            ["User-Agent"] = Ua,
            ["Accept"] = "text/html,application/xhtml+xml",
        }, 15);
        resp.EnsureSuccess();
        var (email, token, exp) = ParseEnPage(resp.Body);
        return new EmailInfo(Channel, email, token, createdAt: exp);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new ArgumentException("token is required for anonbox channel");
        var path = token.EndsWith("/") ? token : token + "/";
        var url = $"{BaseUrl}/{path}";
        var resp = Http.Get(url, new Dictionary<string, string>
        {
            ["User-Agent"] = Ua,
            ["Accept"] = "text/plain,*/*",
        }, 15);
        if (resp.StatusCode == 404) return new List<Email>();
        resp.EnsureSuccess();
        var raw = resp.Body.Trim();
        var result = new List<Email>();
        if (raw.Length == 0) return result;
        foreach (var b in MboxSplitRe.Split(raw))
        {
            var block = b.Trim();
            if (block.Length == 0) continue;
            result.Add(Normalize.NormalizeEmail(MboxBlockToRaw(block, email), email));
        }
        return result;
    }
}
