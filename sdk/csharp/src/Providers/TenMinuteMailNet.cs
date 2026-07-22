using System;
using System.Collections.Generic;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// 10minutemail.net 渠道。
/// 创建：GET / 获取 session cookie（PHPSESSID）+ 从首页输入框提取随机邮箱地址。
/// 收信：GET /mailbox.ajax.php?_={ms} 取列表 HTML 表格，逐封 GET /readmail.html?mid={id} 取正文。
/// 支持 Cloudflare 邮箱保护（data-cfemail）解码。
/// </summary>
public static class TenMinuteMailNet
{
    private const string Channel = "10minutemail-net";
    private const string BaseUrl = "https://10minutemail.net";
    private const string TokPrefix = "tmn1:";

    private const string Ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static readonly Regex EmailRe = new("value=\"([^\"]+@[^\"]+)\"", RegexOptions.Compiled);
    private static readonly Regex RowRe = new("<tr[^>]*>(.*?)</tr>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex MidRe = new("readmail\\.html\\?mid=([^'\"&]+)", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex CellRe = new("<td[^>]*>(.*?)</td>", RegexOptions.IgnoreCase | RegexOptions.Singleline | RegexOptions.Compiled);
    private static readonly Regex TitleRe = new("title=\"([^\"]+)\"", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex CfRe = new("data-cfemail=\"([0-9a-fA-F]+)\"", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex TagRe = new("<[^>]+>", RegexOptions.Singleline | RegexOptions.Compiled);

    private static Dictionary<string, string> BrowserHeaders() => new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
        ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
    };

    private static Dictionary<string, string> AjaxHeaders() => new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "*/*",
        ["Accept-Language"] = "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7",
        ["X-Requested-With"] = "XMLHttpRequest",
        ["Referer"] = $"{BaseUrl}/",
    };

    private static string EncodeToken(string cookieHdr)
    {
        var raw = Encoding.UTF8.GetBytes(Json.Serialize(new Dictionary<string, object?> { ["c"] = cookieHdr }));
        return TokPrefix + Convert.ToBase64String(raw);
    }

    private static string DecodeToken(string token)
    {
        if (!token.StartsWith(TokPrefix)) return "";
        try
        {
            var data = Convert.FromBase64String(token[TokPrefix.Length..]);
            var o = Json.Parse(Encoding.UTF8.GetString(data)) as JsonObject;
            return Json.Str(o, "c").Trim();
        }
        catch { return ""; }
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{BaseUrl}/", BrowserHeaders());
        resp.EnsureSuccess();
        var cookieHdr = ProviderHttpUtil.CookieHeaderFrom(resp);

        var m = EmailRe.Match(resp.Body);
        if (!m.Success) throw new Exception("10minutemail-net: 未能从首页提取邮箱地址");
        var email = m.Groups[1].Value.Trim();
        if (email.Length == 0 || !email.Contains('@')) throw new Exception($"10minutemail-net: 获取到的邮箱地址无效: {email}");

        return new EmailInfo(Channel, email, EncodeToken(cookieHdr));
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string email, string? token)
    {
        email = (email ?? "").Trim();
        if (email.Length == 0) throw new ArgumentException("10minutemail-net: 邮箱地址为空");
        var cookieHdr = DecodeToken((token ?? "").Trim());

        var headers = AjaxHeaders();
        if (cookieHdr.Length > 0) headers["Cookie"] = cookieHdr;
        var listUrl = $"{BaseUrl}/mailbox.ajax.php?_={DateTimeOffset.UtcNow.ToUnixTimeMilliseconds()}";
        var resp = Http.Get(listUrl, headers);
        resp.EnsureSuccess();

        var outList = new List<Email>();
        foreach (Match rowMatch in RowRe.Matches(resp.Body))
        {
            var rowFull = rowMatch.Value;
            var rowInner = rowMatch.Groups[1].Value;
            if (rowInner.ToLowerInvariant().Contains("<th")) continue;

            var midMatch = MidRe.Match(rowInner);
            if (!midMatch.Success) continue;
            var mailId = midMatch.Groups[1].Value.Trim();
            if (mailId.Length == 0) continue;

            var cells = new List<string>();
            foreach (Match c in CellRe.Matches(rowInner)) cells.Add(c.Groups[1].Value);
            var fromCell = cells.Count > 0 ? cells[0] : "";
            var subjectCell = cells.Count > 1 ? cells[1] : "";
            var dateCell = cells.Count > 2 ? cells[2] : "";

            var fromAddr = ExtractText(fromCell);
            var subject = ExtractText(subjectCell);

            var tm = TitleRe.Match(dateCell);
            var date = tm.Success ? tm.Groups[1].Value.Trim() : ExtractText(dateCell);

            var isRead = !rowFull.ToLowerInvariant().Contains("font-weight: bold");

            outList.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = mailId,
                ["from"] = fromAddr,
                ["to"] = email,
                ["subject"] = subject,
                ["html"] = FetchBody(cookieHdr, mailId),
                ["date"] = date,
                ["isRead"] = isRead,
            }, email));
        }
        return outList;
    }

    private static string FetchBody(string cookieHdr, string mailId)
    {
        if (mailId.Length == 0) return "";
        var headers = BrowserHeaders();
        headers["Referer"] = $"{BaseUrl}/";
        if (cookieHdr.Length > 0) headers["Cookie"] = cookieHdr;
        HttpResult resp;
        try { resp = Http.Get($"{BaseUrl}/readmail.html?mid={mailId}", headers); }
        catch { return ""; }
        if (resp.StatusCode is < 200 or >= 300) return "";
        return ExtractBody(resp.Body);
    }

    private static string ExtractBody(string page)
    {
        const string startMark = "class=\"mailinhtml\"";
        var si = page.IndexOf(startMark, StringComparison.Ordinal);
        if (si < 0) return "";
        var gt = page.IndexOf('>', si);
        if (gt < 0) return "";
        var rest = page[(gt + 1)..];

        var ei = rest.IndexOf("email-decode.min.js", StringComparison.Ordinal);
        if (ei < 0)
        {
            var di = rest.IndexOf("</div>", StringComparison.Ordinal);
            return di < 0 ? rest.Trim() : rest[..di].Trim();
        }

        var segment = rest[..ei];
        var sIdx = segment.LastIndexOf("<script", StringComparison.Ordinal);
        if (sIdx >= 0) segment = segment[..sIdx];
        segment = segment.Trim();
        for (var i = 0; i < 2; i++)
        {
            segment = segment.Trim();
            if (segment.EndsWith("</div>")) segment = segment[..^"</div>".Length];
        }
        return segment.Trim();
    }

    private static string ExtractText(string cell)
    {
        var cf = CfRe.Match(cell);
        if (cf.Success)
        {
            var decoded = CfDecode(cf.Groups[1].Value);
            if (decoded.Length > 0) return decoded;
        }
        var text = TagRe.Replace(cell, "");
        foreach (var (ent, ch) in new[]
                 {
                     ("&nbsp;", " "), ("&#160;", " "), ("&amp;", "&"),
                     ("&lt;", "<"), ("&gt;", ">"), ("&quot;", "\""),
                 })
            text = text.Replace(ent, ch);
        return text.Trim();
    }

    private static string CfDecode(string encoded)
    {
        byte[] data;
        try
        {
            data = new byte[encoded.Length / 2];
            for (var i = 0; i < data.Length; i++)
                data[i] = Convert.ToByte(encoded.Substring(i * 2, 2), 16);
        }
        catch { return ""; }
        if (data.Length < 2) return "";
        var key = data[0];
        var sb = new byte[data.Length - 1];
        for (var i = 1; i < data.Length; i++) sb[i - 1] = (byte)(data[i] ^ key);
        var decoded = Encoding.UTF8.GetString(sb);
        return decoded.Contains('@') ? decoded : "";
    }
}
