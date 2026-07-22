using System;
using System.Collections.Generic;
using System.Net;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// mail.zhujump.com 固定域渠道家族。
/// 流程：注册账号 → 取 CSRF → 登录换取会话 Cookie → 校验会话 → 按固定域创建邮箱，
/// token 内嵌 cookie / email_id / base_url 供后续增量取信。
/// lyhlevi.com 等实例复用同一套 NextAuth 协议，仅 base_url 与过期时间不同。
/// </summary>
public static class Zhujump
{
    private const string BaseUrl = "https://mail.zhujump.com";
    private const string TokenPrefix = "zhj1:";
    private const long DefaultExpiryTime = 60L * 60 * 1000;
    private static readonly Random Rand = new();

    private const string Ua =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

    private static string LoginReferer(string baseUrl) => $"{baseUrl}/zh-CN/login";

    private static Dictionary<string, string> BaseHeaders(string baseUrl) => new()
    {
        ["Accept"] = "application/json",
        ["Origin"] = baseUrl,
        ["Referer"] = LoginReferer(baseUrl),
        ["User-Agent"] = Ua,
    };

    private static Dictionary<string, string> WithCookie(string baseUrl, string cookie)
    {
        var h = BaseHeaders(baseUrl);
        h["Cookie"] = cookie;
        return h;
    }

    private static string RandomValue(string prefix, int size)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder(prefix);
        for (var i = 0; i < size; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string RandomPassword() => "Sdk!" + RandomValue("", 12) + "A1";

    private static string EncodeToken(string cookie, string emailId, string baseUrl)
    {
        var raw = Json.Serialize(new Dictionary<string, object?>
        {
            ["c"] = cookie,
            ["i"] = emailId,
            ["b"] = baseUrl,
        });
        return TokenPrefix + Convert.ToBase64String(Encoding.UTF8.GetBytes(raw));
    }

    private static (string Cookie, string EmailId, string BaseUrl) DecodeToken(string? token)
    {
        if (string.IsNullOrEmpty(token) || !token.StartsWith(TokenPrefix, StringComparison.Ordinal))
            throw new Exception("zhujump: invalid session token");
        JsonObject? data;
        try
        {
            var raw = Convert.FromBase64String(token[TokenPrefix.Length..]);
            data = Json.Parse(Encoding.UTF8.GetString(raw)) as JsonObject;
        }
        catch { throw new Exception("zhujump: invalid session token"); }

        var cookie = Json.Str(data, "c").Trim();
        var emailId = Json.Str(data, "i").Trim();
        if (cookie.Length == 0 || emailId.Length == 0) throw new Exception("zhujump: invalid session token");
        var baseUrl = Json.Str(data, "b").Trim().TrimEnd('/');
        if (baseUrl.Length == 0) baseUrl = BaseUrl;
        return (cookie, emailId, baseUrl);
    }

    /// <summary>创建 zhujump 临时邮箱（默认实例）</summary>
    public static EmailInfo Generate(string domain, string channel)
        => GenerateForInstance(BaseUrl, domain, channel, DefaultExpiryTime);

    /// <summary>创建 zhujump 临时邮箱（指定实例 base_url 与过期时间；expiryTime 为 null 时不传）</summary>
    public static EmailInfo GenerateForInstance(string baseUrl, string domain, string channel, long? expiryTime)
    {
        baseUrl = baseUrl.TrimEnd('/');
        var username = RandomValue("sdk", 10);
        var password = RandomPassword();
        var cookie = "";

        // 1. 注册账号
        var regBody = Json.Serialize(new Dictionary<string, object?>
        {
            ["username"] = username,
            ["password"] = password,
            ["turnstileToken"] = "",
        });
        var reg = Http.Post($"{baseUrl}/api/auth/register", regBody, "application/json", WithCookie(baseUrl, cookie));
        reg.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, reg);

        // 2. 取 CSRF token
        var csrfResp = Http.Get($"{baseUrl}/api/auth/csrf", WithCookie(baseUrl, cookie));
        csrfResp.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, csrfResp);
        var csrf = Json.Str(Json.Parse(csrfResp.Body), "csrfToken").Trim();
        if (csrf.Length == 0) throw new Exception("zhujump: csrf token missing");

        // 3. 登录（不跟随重定向以捕获会话 Cookie）
        var loginForm =
            $"username={WebUtility.UrlEncode(username)}" +
            $"&password={WebUtility.UrlEncode(password)}" +
            "&turnstileToken=" +
            "&redirect=false" +
            $"&csrfToken={WebUtility.UrlEncode(csrf)}" +
            $"&callbackUrl={WebUtility.UrlEncode(LoginReferer(baseUrl))}";
        var loginHeaders = WithCookie(baseUrl, cookie);
        loginHeaders["x-auth-return-redirect"] = "1";
        var login = Http.PostNoRedirect($"{baseUrl}/api/auth/callback/credentials?", loginForm,
            "application/x-www-form-urlencoded", loginHeaders);
        login.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, login);

        // 4. 校验会话
        var sessionResp = Http.Get($"{baseUrl}/api/auth/session", WithCookie(baseUrl, cookie));
        sessionResp.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, sessionResp);
        var sessionJson = Json.Parse(sessionResp.Body) as JsonObject;
        var user = sessionJson?["user"] as JsonObject;
        if (Json.Str(user, "username").Trim() != username)
            throw new Exception("zhujump: login verification failed");

        // 5. 创建邮箱
        var local = RandomValue("sdk", 10);
        var genBody = new Dictionary<string, object?> { ["name"] = local, ["domain"] = domain };
        if (expiryTime is not null) genBody["expiryTime"] = expiryTime.Value;
        var created = Http.Post($"{baseUrl}/api/emails/generate", Json.Serialize(genBody),
            "application/json", WithCookie(baseUrl, cookie));
        created.EnsureSuccess();
        cookie = ProviderHttpUtil.MergeCookies(cookie, created);
        var createdJson = Json.Parse(created.Body) as JsonObject;
        var email = Json.Str(createdJson, "email").Trim();
        var emailId = Json.Str(createdJson, "id").Trim();
        if (email.Length == 0 || emailId.Length == 0) throw new Exception("zhujump: invalid generate response");

        return new EmailInfo(channel, email, EncodeToken(cookie, emailId, baseUrl));
    }

    private static JsonObject? FetchDetail(string baseUrl, string cookie, string emailId, string messageId)
    {
        var resp = Http.Get($"{baseUrl}/api/emails/{emailId}/{messageId}", WithCookie(baseUrl, cookie));
        if (!resp.Ok) return null;
        var data = Json.Parse(resp.Body) as JsonObject;
        return data?["message"] as JsonObject;
    }

    /// <summary>获取 zhujump 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        var (cookie, emailId, baseUrl) = DecodeToken(token);
        var resp = Http.Get($"{baseUrl}/api/emails/{emailId}", WithCookie(baseUrl, cookie));
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;
        var rows = data?["messages"] as JsonArray;
        var result = new List<Email>();
        if (rows is null) return result;

        foreach (var item in rows)
        {
            if (item is not JsonObject io) continue;
            JsonObject source = io;
            var messageId = Json.Str(io, "id").Trim();
            var hasBody = Json.Str(io, "content").Trim().Length > 0 || Json.Str(io, "html").Trim().Length > 0;
            if (messageId.Length > 0 && !hasBody)
            {
                var detail = FetchDetail(baseUrl, cookie, emailId, messageId);
                if (detail is not null)
                {
                    // 合并列表项与详情（详情覆盖）
                    var merged = new JsonObject();
                    foreach (var kv in io) merged[kv.Key] = kv.Value?.DeepClone();
                    foreach (var kv in detail) merged[kv.Key] = kv.Value?.DeepClone();
                    source = merged;
                }
            }

            var toAddr = Json.Str(source, "to_address");
            var received = Json.Str(source, "received_at");
            if (received.Length == 0) received = Json.Str(source, "sent_at");
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(source, "id"),
                ["from_address"] = Json.Str(source, "from_address"),
                ["to_address"] = string.IsNullOrEmpty(toAddr) ? email : toAddr,
                ["subject"] = Json.Str(source, "subject"),
                ["content"] = Json.Str(source, "content"),
                ["html"] = Json.Str(source, "html"),
                ["received_at"] = received,
                ["isRead"] = false,
            }, email));
        }
        return result;
    }
}
