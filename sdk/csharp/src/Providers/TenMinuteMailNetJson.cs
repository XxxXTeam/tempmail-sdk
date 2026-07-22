using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// ten-minute-mail-net 渠道 — https://10minutemail.net（address.api.php JSON 接口）。
/// 注意：与 <see cref="TenMinuteMailNet"/>（10minutemail-net，HTML 抓取）为不同渠道 slug。
/// 创建：GET /address.api.php 返回 mail_get_mail 地址，从 cookie 提取 PHPSESSID，
///       token 序列化为 {"cookie":"PHPSESSID=xxx"}。
/// 收信：GET /address.api.php 取 mail_list，逐封 GET /mail.api.php?mailid={id} 取正文。
/// </summary>
public static class TenMinuteMailNetJson
{
    private const string BaseUrl = "https://10minutemail.net";
    private static readonly Dictionary<string, string> Headers = new()
    {
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36",
        ["Accept"] = "application/json",
    };

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Get($"{BaseUrl}/address.api.php", new Dictionary<string, string>(Headers));
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var address = Json.Str(data, "mail_get_mail");
        if (string.IsNullOrEmpty(address)) throw new Exception("ten-minute-mail-net: 创建邮箱失败");

        var phpsessid = ProviderHttpUtil.ExtractCookieValue(resp, "PHPSESSID");
        if (string.IsNullOrEmpty(phpsessid)) throw new Exception("ten-minute-mail-net: 未获取到 PHPSESSID cookie");
        var token = Json.Serialize(new Dictionary<string, object?> { ["cookie"] = $"PHPSESSID={phpsessid}" });

        long? expires = null;
        if (data!.TryGetPropertyValue("mail_get_duetime", out var dt) && dt is not null
            && long.TryParse(dt.ToString(), out var due)) expires = due * 1000;

        return new EmailInfo("ten-minute-mail-net", address, token, expires);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var cookieStr = Json.Str(Json.Parse(token ?? ""), "cookie");
        var headers = new Dictionary<string, string>(Headers) { ["Cookie"] = cookieStr };

        var resp = Http.Get($"{BaseUrl}/address.api.php", headers);
        resp.EnsureSuccess();
        var mailList = (Json.Parse(resp.Body) as JsonObject)?["mail_list"] as JsonArray;
        var result = new List<Email>();
        if (mailList is null) return result;

        foreach (var m in mailList)
        {
            if (m is not JsonObject item) continue;
            var mailId = Json.Str(item, "mail_id");
            if (string.IsNullOrEmpty(mailId) || mailId == "welcome") continue;

            var detailResp = Http.Get($"{BaseUrl}/mail.api.php?mailid={mailId}", headers);
            detailResp.EnsureSuccess();
            if (Json.Parse(detailResp.Body) is not JsonObject detail) continue;

            var htmlBody = "";
            var textBody = "";
            if (detail["body"] is JsonArray parts)
            {
                foreach (var p in parts)
                {
                    if (p is not JsonObject part) continue;
                    var contentType = Json.Str(part, "content");
                    if (contentType.Contains("text/html")) htmlBody = Json.Str(part, "body");
                    else if (contentType.Contains("text/plain")) textBody = Json.Str(part, "body");
                }
            }
            if (string.IsNullOrEmpty(textBody) && detail["plain"] is JsonArray plainList && plainList.Count > 0)
                textBody = Json.NodeToString(plainList[0]);

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = mailId,
                ["from"] = !string.IsNullOrEmpty(Json.Str(detail, "from")) ? Json.Str(detail, "from") : Json.Str(item, "from"),
                ["to"] = !string.IsNullOrEmpty(Json.Str(detail, "to")) ? Json.Str(detail, "to") : email,
                ["subject"] = !string.IsNullOrEmpty(Json.Str(detail, "subject")) ? Json.Str(detail, "subject") : Json.Str(item, "subject"),
                ["text"] = textBody,
                ["html"] = htmlBody,
                ["date"] = !string.IsNullOrEmpty(Json.Str(detail, "datetime")) ? Json.Str(detail, "datetime") : Json.Str(item, "datetime"),
            }, email));
        }
        return result;
    }
}
