using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Mailinator 渠道家族实现（mailinator.com 及其众多姊妹域名）。
/// 所有姊妹域名的 MX 都指向 mail.mailinator.com，收信统一走
/// domain=public 的公共收件箱 API，因此姊妹域仅在 generate 时替换域名，
/// getEmails 逻辑完全复用主域。
/// API: https://mailinator.com/api/v2/domains/public/...
/// </summary>
public static class Mailinator
{
    private static readonly Random Rand = new();

    private const string BaseUrl = "https://mailinator.com";
    private const string PublicDomain = "public";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "application/json",
        ["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["Cache-Control"] = "no-cache",
        ["Pragma"] = "no-cache",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
    };

    /// <summary>生成 12 位随机小写字母/数字本地名</summary>
    private static string RandomString(int length)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>创建主域 mailinator.com 临时邮箱</summary>
    public static EmailInfo Generate() => Generate("mailinator", "mailinator.com");

    /// <summary>创建指定姊妹域名的临时邮箱（channel/domain 由注册表传入）</summary>
    public static EmailInfo Generate(string channel, string domain)
        => new(channel, $"{RandomString(12)}@{domain}");

    /// <summary>请求 JSON 并返回对象节点；非 2xx 或非对象时统一封装/返回 null</summary>
    private static JsonObject? RequestJson(string url)
    {
        try
        {
            var resp = Http.Get(url, Headers, timeout: 15);
            if (!resp.Ok) return null;
            var node = Json.Parse(resp.Body);
            if (node is JsonObject obj) return obj;
            // 非对象响应（如数组）包装进 { "data": ... }，与 Python 一致
            if (node is not null) return new JsonObject { ["data"] = node.DeepClone() };
            return null;
        }
        catch
        {
            return null;
        }
    }

    /// <summary>从收件箱响应中解析消息摘要列表（兼容顶层数组 / msgs / data）</summary>
    private static List<JsonObject> ParseMessages(JsonObject? data)
    {
        var result = new List<JsonObject>();
        if (data is null) return result;

        // RequestJson 已把顶层数组包成 { data: [...] }
        foreach (var key in new[] { "msgs", "data" })
        {
            if (data[key] is JsonArray arr)
            {
                foreach (var item in arr)
                    if (item is JsonObject o) result.Add(o);
                if (result.Count > 0) return result;
            }
        }
        return result;
    }

    /// <summary>获取收件箱邮件（token 无关；email 取本地名查询 domain=public 收件箱）</summary>
    public static List<Email> GetEmails(string email)
    {
        var result = new List<Email>();
        var inbox = (email ?? "").Trim();
        var at = inbox.IndexOf('@');
        if (at >= 0) inbox = inbox.Substring(0, at);
        if (string.IsNullOrEmpty(inbox)) return result;

        var data = RequestJson($"{BaseUrl}/api/v2/domains/{PublicDomain}/inboxes/{inbox}");
        var messages = ParseMessages(data);
        if (messages.Count == 0) return result;

        foreach (var msg in messages)
        {
            var messageId = FirstStr(msg, "id", "messageId").Trim();

            JsonObject? textPayload = null, htmlPayload = null, attachmentsPayload = null;
            if (!string.IsNullOrEmpty(messageId))
            {
                textPayload = RequestJson($"{BaseUrl}/api/v2/domains/{PublicDomain}/messages/{messageId}/text");
                htmlPayload = RequestJson($"{BaseUrl}/api/v2/domains/{PublicDomain}/messages/{messageId}/texthtml");
                attachmentsPayload = RequestJson($"{BaseUrl}/api/v2/domains/{PublicDomain}/messages/{messageId}/attachments");
            }

            result.Add(Normalize.NormalizeEmail(
                Flatten(msg, email ?? "", textPayload, htmlPayload, attachmentsPayload),
                email ?? ""));
        }
        return result;
    }

    /// <summary>把摘要 + 详情三段响应拍平为 Normalize 消费的原始字段字典</summary>
    private static Dictionary<string, object?> Flatten(
        JsonObject summary, string recipientEmail,
        JsonObject? textPayload, JsonObject? htmlPayload, JsonObject? attachmentsPayload)
    {
        var attachments = new List<object?>();
        if (attachmentsPayload?["attachments"] is JsonArray arr)
        {
            foreach (var item in arr)
            {
                if (item is not JsonObject a) continue;
                attachments.Add(new Dictionary<string, object?>
                {
                    ["filename"] = FirstOrNull(a, "name", "filename"),
                    ["size"] = FirstOrNull(a, "size", "filesize"),
                    ["contentType"] = FirstOrNull(a, "contentType", "content_type", "mimeType", "mime_type"),
                    ["url"] = AttachmentUrl(FirstStr(a, "downloadUrl", "url")),
                });
            }
        }

        var to = FirstStr(summary, "to");
        if (string.IsNullOrEmpty(to)) to = recipientEmail;

        return new Dictionary<string, object?>
        {
            ["id"] = FirstStr(summary, "id", "messageId"),
            ["from"] = FirstStr(summary, "from", "origfrom"),
            ["to"] = to,
            ["subject"] = FirstStr(summary, "subject"),
            ["text"] = TextFromPayload(textPayload, "text/plain"),
            ["html"] = TextFromPayload(htmlPayload, "text/html"),
            ["date"] = ToIsoTime(summary["time"] ?? summary["date"]),
            ["seen"] = false,
            ["attachments"] = attachments,
        };
    }

    /// <summary>取 payload 指定键的字符串正文（缺失返回空串）</summary>
    private static string TextFromPayload(JsonObject? payload, string key)
    {
        if (payload is null) return "";
        return Json.Str(payload, key);
    }

    /// <summary>补全附件下载 URL 的绝对前缀</summary>
    private static string AttachmentUrl(string value)
    {
        if (string.IsNullOrEmpty(value)) return "";
        if (value.StartsWith("http://") || value.StartsWith("https://")) return value;
        return $"{BaseUrl}{value}";
    }

    /// <summary>把时间字段（毫秒/秒时间戳或字符串）转为 ISO8601（Z 结尾）</summary>
    private static string ToIsoTime(JsonNode? value)
    {
        if (value is null) return "";
        if (value is JsonValue val)
        {
            if (val.TryGetValue<double>(out var num) && num != 0)
            {
                var millis = num > 1e12 ? (long)num : (long)(num * 1000);
                return DateTimeOffset.FromUnixTimeMilliseconds(millis)
                    .ToUniversalTime()
                    .ToString("yyyy-MM-ddTHH:mm:ss.fffffffK", CultureInfo.InvariantCulture)
                    .Replace("+00:00", "Z");
            }
            if (val.TryGetValue<string>(out var text) && !string.IsNullOrEmpty(text))
            {
                var normalized = text.Replace("Z", "+00:00");
                if (DateTimeOffset.TryParse(normalized, CultureInfo.InvariantCulture,
                        DateTimeStyles.RoundtripKind, out var dto))
                    return dto.ToUniversalTime().ToString("o", CultureInfo.InvariantCulture)
                        .Replace("+00:00", "Z");
                return text;
            }
        }
        return Json.NodeToString(value);
    }

    /// <summary>依序取多个键的字符串值，全缺返回空串</summary>
    private static string FirstStr(JsonObject obj, params string[] keys)
    {
        foreach (var k in keys)
        {
            var s = Json.Str(obj, k);
            if (!string.IsNullOrEmpty(s)) return s;
        }
        return "";
    }

    /// <summary>依序取多个键的原始节点（转 object?），全缺返回 null</summary>
    private static object? FirstOrNull(JsonObject obj, params string[] keys)
    {
        foreach (var k in keys)
            if (obj.TryGetPropertyValue(k, out var v) && v is not null)
                return Json.ToRaw(v);
        return null;
    }
}
