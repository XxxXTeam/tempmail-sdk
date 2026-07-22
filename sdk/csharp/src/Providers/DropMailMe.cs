using System;
using System.Collections.Generic;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// dropmail-me 渠道 — https://dropmail.me。
/// 自行生成认证 token：从页面提取 data-k，反转 + base64 解码得 secret，
/// 用 random_part + secret 经 FNV-1a 变体哈希构造 website_ 令牌；
/// 随后 GraphQL introduceSession 建会话，session(id) 查询邮件。
/// token 序列化为 {"session_id","auth_token"} JSON。
/// </summary>
public static class DropMailMe
{
    private const string Channel = "dropmail-me";
    private const string BaseUrl = "https://dropmail.me";

    private const string Ua =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

    private static readonly Dictionary<string, string> Headers = new()
    {
        ["User-Agent"] = Ua,
        ["Accept"] = "application/json",
        ["Content-Type"] = "application/json",
    };

    private static readonly Regex DataKRe = new("<meta\\s+name=\"app-config\"\\s+data-k=\"([^\"]+)\"", RegexOptions.Compiled);
    private static readonly Random Rand = new();

    /// <summary>FNV-1a 变体哈希（与 Python 参考逐位对齐，输出小写十六进制）</summary>
    private static string FnvHash(string s)
    {
        uint h = 2166136261;
        foreach (var ch in s)
        {
            h ^= ch;
            h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24);
        }
        return h.ToString("x");
    }

    private static string GenerateAuthToken()
    {
        var resp = Http.Get($"{BaseUrl}/en/", new Dictionary<string, string> { ["User-Agent"] = Ua, ["Accept"] = "text/html" }, 15);
        resp.EnsureSuccess();
        var match = DataKRe.Match(resp.Body);
        if (!match.Success) throw new Exception("dropmail-me: 无法从页面提取 data-k");

        var dataK = match.Groups[1].Value;
        var reversed = new string(dataK.ToCharArray()); // 先反转
        var arr = reversed.ToCharArray();
        Array.Reverse(arr);
        var secret = Encoding.UTF8.GetString(Convert.FromBase64String(new string(arr)));

        var dateStr = DateTime.UtcNow.ToString("yyyyMMdd");
        const string alnum = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        var sb = new StringBuilder(dateStr);
        for (var i = 0; i < 16; i++) sb.Append(alnum[Rand.Next(alnum.Length)]);
        var randomPart = sb.ToString();

        var h = FnvHash(randomPart + secret);
        return $"website_{randomPart}_{h}";
    }

    /// <summary>创建临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var authToken = GenerateAuthToken();

        const string query = "mutation { introduceSession { id expiresAt addresses { address } } }";
        var resp = Http.Post($"{BaseUrl}/api/graphql/{authToken}",
            Json.Serialize(new Dictionary<string, object?> { ["query"] = query }), "application/json", Headers, 15);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;

        var session = (data?["data"] as JsonObject)?["introduceSession"] as JsonObject;
        if (session is null) throw new Exception($"dropmail-me: 创建 session 失败，响应: {resp.Body}");

        var sessionId = Json.Str(session, "id");
        var addresses = session["addresses"] as JsonArray;
        if (sessionId.Length == 0 || addresses is null || addresses.Count == 0)
            throw new Exception($"dropmail-me: 响应中缺少 session ID 或地址，响应: {resp.Body}");

        var address = Json.Str(addresses[0] as JsonObject, "address");
        if (address.Length == 0) throw new Exception($"dropmail-me: 地址为空，响应: {resp.Body}");

        var compositeToken = Json.Serialize(new Dictionary<string, object?>
        {
            ["session_id"] = sessionId,
            ["auth_token"] = authToken,
        });

        long? expiresAt = null;
        var expStr = Json.Str(session, "expiresAt");
        if (expStr.Length > 0 && DateTimeOffset.TryParse(expStr.Replace("Z", "+00:00"), out var dto))
            expiresAt = dto.ToUnixTimeMilliseconds();

        return new EmailInfo(Channel, address, compositeToken, expiresAt);
    }

    /// <summary>获取邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new ArgumentException("dropmail-me: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new ArgumentException("dropmail-me: 邮箱地址不能为空");

        var tokenData = Json.Parse(token) as JsonObject
            ?? throw new ArgumentException("dropmail-me: token 格式无效，应为 JSON 字符串");
        var sessionId = Json.Str(tokenData, "session_id");
        var authToken = Json.Str(tokenData, "auth_token");
        if (sessionId.Length == 0 || authToken.Length == 0)
            throw new ArgumentException("dropmail-me: token 中缺少 session_id 或 auth_token 字段");

        var query = "{ session(id:\"" + sessionId + "\") { mails { id headerFrom headerSubject text html receivedAt } } }";
        var resp = Http.Post($"{BaseUrl}/api/graphql/{authToken}",
            Json.Serialize(new Dictionary<string, object?> { ["query"] = query }), "application/json", Headers, 15);
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject;

        var session = (data?["data"] as JsonObject)?["session"] as JsonObject;
        var mails = session?["mails"] as JsonArray;
        var result = new List<Email>();
        if (mails is null) return result;
        foreach (var m in mails)
        {
            if (m is not JsonObject mo) continue;
            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(mo, "id"),
                ["from"] = Json.Str(mo, "headerFrom"),
                ["to"] = addr,
                ["subject"] = Json.Str(mo, "headerSubject"),
                ["text"] = Json.Str(mo, "text"),
                ["html"] = Json.Str(mo, "html"),
                ["date"] = Json.Str(mo, "receivedAt"),
            }, addr));
        }
        return result;
    }
}
