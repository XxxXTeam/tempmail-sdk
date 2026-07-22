using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// smail.pw 渠道（http_style: other）。
/// 创建：POST /_root.data intent=generate，从 __session cookie 与响应文本正则提取邮箱。
/// 收信：GET /_root.data（带 cookie），响应为 Remix Flight 序列化文本；
/// 通过多组正则 + JSON Flight 结构解析出邮件行，再逐封 GET /api/email/{id} 补正文。
/// </summary>
public static class SmailPw
{
    private const string Channel = "smail-pw";
    private const string RootDataUrl = "https://smail.pw/_root.data";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["Accept"] = "*/*",
        ["accept-language"] = "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
        ["cache-control"] = "no-cache",
        ["dnt"] = "1",
        ["origin"] = "https://smail.pw",
        ["pragma"] = "no-cache",
        ["referer"] = "https://smail.pw/",
        ["sec-ch-ua"] = "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"",
        ["sec-ch-ua-mobile"] = "?0",
        ["sec-ch-ua-platform"] = "\"Windows\"",
        ["sec-fetch-dest"] = "empty",
        ["sec-fetch-mode"] = "cors",
        ["sec-fetch-site"] = "same-origin",
    };

    private static readonly Regex QuotedInboxRe = new("\"([a-z0-9][a-z0-9.-]*@smail\\.pw)\"", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex PlainInboxRe = new("\\b([a-z0-9][a-z0-9.-]*@smail\\.pw)\\b", RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex MailRe = new(
        "\"id\",\"([^\"]+)\",\"to_address\",\"([^\"]*)\",\"from_name\",\"([^\"]*)\",\"from_address\",\"([^\"]*)\",\"subject\",\"([^\"]*)\",\"time\",(\\d+)", RegexOptions.Compiled);
    private static readonly Regex MailNoSubjRe = new(
        "\"id\",\"([^\"]+)\",\"to_address\",\"([^\"]*)\",\"from_name\",\"([^\"]*)\",\"from_address\",\"([^\"]*)\",\"subject\",\"time\",(\\d+)", RegexOptions.Compiled);
    private static readonly Regex Mail2Re = new(
        "\"id\",\"([^\"]+)\",\"from_name\",\"([^\"]*)\",\"from_address\",\"([^\"]*)\",\"subject\",\"([^\"]*)\",\"time\",(\\d+)", RegexOptions.Compiled);

    private static string SessionCookie(HttpResult resp)
    {
        var v = ProviderHttpUtil.ExtractCookieValue(resp, "__session");
        return v.Length > 0 ? $"__session={v}" : "";
    }

    private static string ExtractInbox(string text)
    {
        var m = QuotedInboxRe.Match(text);
        if (m.Success) return m.Groups[1].Value;
        m = PlainInboxRe.Match(text);
        return m.Success ? m.Groups[1].Value : "";
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var resp = Http.Post(RootDataUrl, "intent=generate",
            "application/x-www-form-urlencoded;charset=UTF-8", Headers);
        resp.EnsureSuccess();
        var cookie = SessionCookie(resp);
        if (cookie.Length == 0) throw new Exception("Failed to extract __session cookie");
        var email = ExtractInbox(resp.Body);
        if (email.Length == 0) throw new Exception("Failed to parse inbox from smail.pw response");
        return new EmailInfo(Channel, email, cookie);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var headers = new Dictionary<string, string>(Headers) { ["Cookie"] = token ?? "" };
        var resp = Http.Get(RootDataUrl, headers);
        resp.EnsureSuccess();
        var rawList = ParseMails(resp.Body, email);
        var outList = new List<Email>();
        foreach (var r in rawList) outList.Add(Normalize.NormalizeEmail(r, email));

        foreach (var item in outList)
        {
            if (string.IsNullOrEmpty(item.Id)) continue;
            var body = FetchEmailBody(token ?? "", item.Id);
            if (!string.IsNullOrEmpty(body)) item.Html = body;
        }
        return outList;
    }

    private static Dictionary<string, object?> MakeRow(string id, string to, string fromName, string fromAddress, string subject, long date)
        => new()
        {
            ["id"] = id,
            ["to_address"] = to,
            ["from_name"] = fromName,
            ["from_address"] = fromAddress,
            ["subject"] = subject,
            ["date"] = date,
            ["text"] = "",
            ["html"] = "",
            ["attachments"] = new List<object?>(),
        };

    private static List<Dictionary<string, object?>> ParseMails(string text, string recipient)
    {
        var chunks = new List<List<Dictionary<string, object?>>>();

        var reg = new List<Dictionary<string, object?>>();
        foreach (Match m in MailRe.Matches(text))
            reg.Add(MakeRow(m.Groups[1].Value,
                m.Groups[2].Value.Length > 0 ? m.Groups[2].Value : recipient,
                m.Groups[3].Value, m.Groups[4].Value, m.Groups[5].Value, long.Parse(m.Groups[6].Value)));
        if (reg.Count > 0) chunks.Add(reg);

        var reg0 = new List<Dictionary<string, object?>>();
        foreach (Match m in MailNoSubjRe.Matches(text))
            reg0.Add(MakeRow(m.Groups[1].Value,
                m.Groups[2].Value.Length > 0 ? m.Groups[2].Value : recipient,
                m.Groups[3].Value, m.Groups[4].Value, "", long.Parse(m.Groups[5].Value)));
        if (reg0.Count > 0) chunks.Add(reg0);

        var reg2 = new List<Dictionary<string, object?>>();
        foreach (Match m in Mail2Re.Matches(text))
            reg2.Add(MakeRow(m.Groups[1].Value, recipient, m.Groups[2].Value, m.Groups[3].Value,
                m.Groups[4].Value, long.Parse(m.Groups[5].Value)));
        if (reg2.Count > 0) chunks.Add(reg2);

        // JSON 解析：普通行对象遍历 + Flight 结构
        try
        {
            var root = Json.Parse(text);
            if (root is not null)
            {
                var plain = new List<Dictionary<string, object?>>();
                WalkPlainRowEmails(root, recipient, plain, new HashSet<JsonNode>());
                if (plain.Count > 0) chunks.Add(plain);
                if (root is JsonArray arr)
                {
                    var flight = ParseFlightRoot(arr, recipient);
                    if (flight.Count > 0) chunks.Add(flight);
                }
            }
        }
        catch { /* 忽略 JSON 解析失败 */ }

        var flat = new List<Dictionary<string, object?>>();
        foreach (var part in chunks) flat.AddRange(part);
        return MergeById(flat);
    }

    private static void WalkPlainRowEmails(JsonNode? node, string recipient, List<Dictionary<string, object?>> outList, HashSet<JsonNode> seen)
    {
        if (node is null) return;
        if (node is not JsonObject && node is not JsonArray) return;
        if (!seen.Add(node)) return;

        if (node is JsonArray arr)
        {
            foreach (var el in arr) WalkPlainRowEmails(el, recipient, outList, seen);
            return;
        }

        var obj = (JsonObject)node;
        if (obj.TryGetPropertyValue("subject", out var subjNode) && subjNode is JsonValue sv && sv.TryGetValue<string>(out var subj))
        {
            long? timeMs = null;
            if (obj.TryGetPropertyValue("time", out var tNode) && tNode is JsonValue tv)
            {
                if (tv.TryGetValue<long>(out var tl)) timeMs = tl;
                else if (tv.TryGetValue<double>(out var td) && !double.IsNaN(td)) timeMs = (long)td;
                else if (tv.TryGetValue<string>(out var ts) && long.TryParse(ts, out var tp)) timeMs = tp;
            }
            if (timeMs is not null)
                outList.Add(MakeRow(Json.Str(obj, "id"),
                    string.IsNullOrEmpty(Json.Str(obj, "to_address")) ? recipient : Json.Str(obj, "to_address"),
                    Json.Str(obj, "from_name"), Json.Str(obj, "from_address"), subj, timeMs.Value));
        }

        foreach (var kv in obj)
            if (kv.Value is JsonObject or JsonArray)
                WalkPlainRowEmails(kv.Value, recipient, outList, seen);
    }

    private static bool IsFlightDeferredRow(JsonNode? node)
    {
        if (node is not JsonObject obj || obj.Count == 0) return false;
        foreach (var kv in obj) if (!kv.Key.StartsWith("_")) return false;
        return true;
    }

    private static Dictionary<string, object?>? ResolveFlightDeferredRow(JsonArray root, JsonObject obj)
    {
        var fields = new Dictionary<string, object?>();
        foreach (var kv in obj)
        {
            if (!kv.Key.StartsWith("_")) continue;
            if (!int.TryParse(kv.Key[1..], out var keyIdx)) continue;
            if (kv.Value is not JsonValue vv || !TryGetInt(vv, out var valIdx)) continue;
            if (keyIdx < 0 || keyIdx >= root.Count || valIdx < 0 || valIdx >= root.Count) continue;
            var keyNode = root[keyIdx];
            var valNode = root[valIdx];
            var key = (keyNode as JsonValue)?.TryGetValue<string>(out var ks) == true ? ks : null;
            if (key is null) continue;
            if (key == "time" && valNode is JsonValue tvn && TryGetInt(tvn, out var tval))
                fields["time"] = (long)tval;
            else if (valNode is JsonValue vsn && vsn.TryGetValue<string>(out var vs))
                fields[key] = vs;
        }
        if (string.IsNullOrEmpty(fields.GetValueOrDefault("id") as string) && !fields.ContainsKey("time")) return null;
        return fields;
    }

    private static bool TryGetInt(JsonValue v, out int result)
    {
        if (v.TryGetValue<long>(out var l)) { result = (int)l; return true; }
        if (v.TryGetValue<int>(out var i)) { result = i; return true; }
        if (v.TryGetValue<string>(out var s) && int.TryParse(s, out var sp)) { result = sp; return true; }
        result = 0;
        return false;
    }

    private static Dictionary<string, object?>? RowFieldsToMail(Dictionary<string, object?> fields, string recipient)
    {
        if (fields.GetValueOrDefault("time") is not long timeMs) return null;
        var toAddr = fields.GetValueOrDefault("to_address") as string ?? recipient;
        var subject = fields.GetValueOrDefault("subject") as string ?? "";
        if (subject == toAddr || subject.EndsWith("@smail.pw")) subject = "";
        return MakeRow(fields.GetValueOrDefault("id") as string ?? "", toAddr,
            fields.GetValueOrDefault("from_name") as string ?? "",
            fields.GetValueOrDefault("from_address") as string ?? "", subject, timeMs);
    }

    private static List<Dictionary<string, object?>> ParseFlightRoot(JsonArray root, string recipient)
    {
        var mails = new List<Dictionary<string, object?>>();
        for (var i = 0; i < root.Count; i++)
        {
            if ((root[i] as JsonValue)?.TryGetValue<string>(out var el) != true || el != "emails" || i + 1 >= root.Count) continue;
            if (root[i + 1] is not JsonArray refs) break;
            foreach (var r in refs)
            {
                int? idx = null;
                if (r is JsonValue rv)
                {
                    if (rv.TryGetValue<long>(out var rl)) idx = (int)rl;
                    else if (rv.TryGetValue<string>(out var rs) && int.TryParse(rs, out var rp)) idx = rp;
                }
                if (idx is null || idx < 0 || idx >= root.Count) continue;
                var node = root[idx.Value];
                if (IsFlightDeferredRow(node))
                {
                    var fields = ResolveFlightDeferredRow(root, (JsonObject)node!);
                    if (fields is not null)
                    {
                        var mail = RowFieldsToMail(fields, recipient);
                        if (mail is not null) mails.Add(mail);
                    }
                }
            }
            break;
        }
        return mails;
    }

    private static List<Dictionary<string, object?>> MergeById(List<Dictionary<string, object?>> rows)
    {
        var byId = new Dictionary<string, Dictionary<string, object?>>();
        var order = new List<string>();
        var anon = 0;
        foreach (var mail in rows)
        {
            var mid = mail.GetValueOrDefault("id") as string ?? "";
            if (mid.Length == 0)
            {
                var subj = mail.GetValueOrDefault("subject") as string ?? "";
                mid = $"__smail_{anon}_{mail.GetValueOrDefault("date")}_{(subj.Length > 48 ? subj[..48] : subj)}";
                mail["id"] = mid;
                anon++;
            }
            if (!byId.ContainsKey(mid)) { byId[mid] = mail; order.Add(mid); }
        }
        var result = new List<Dictionary<string, object?>>();
        foreach (var k in order) result.Add(byId[k]);
        return result;
    }

    private static string FetchEmailBody(string token, string mailId)
    {
        if (string.IsNullOrEmpty(mailId) || mailId.StartsWith("__smail_")) return "";
        try
        {
            var resp = Http.Get($"https://smail.pw/api/email/{mailId}",
                new Dictionary<string, string>(Headers) { ["Cookie"] = token, ["Accept"] = "application/json" });
            if (resp.StatusCode != 200) return "";
            var data = Json.Parse(resp.Body) as JsonObject;
            return Json.Str(data, "body");
        }
        catch { return ""; }
    }
}
