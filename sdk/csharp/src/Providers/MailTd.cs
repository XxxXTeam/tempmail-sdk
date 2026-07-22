using System;
using System.Collections.Generic;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// Mail.TD 渠道 — https://mail.td（API https://api.mail.td/api）
/// 使用 SHA-256 Proof-of-Work（工作量证明）创建账户。
/// 获取域名：GET /domains → 过滤 pro_only。
/// 创建账户：POST /accounts body JSON{address,auth_key,pow{t,n,d}}；201 成功或 status=retry 提升难度重试。
/// 收信：GET /accounts/{id}/messages?page=1 头 Authorization: Bearer {jwt}。
/// token 存 JSON {"jwt","id"}。
/// </summary>
public static class MailTd
{
    private const string BaseUrl = "https://api.mail.td/api";

    // 初始 PoW 难度（前导零位数）
    private const int InitialDifficulty = 15;
    // PoW 难度上限，避免服务端要求过高难度导致本地长时间计算
    private const int MaxDifficulty = 24;
    // 创建账户时因 PoW 难度提升而重试的最大次数
    private const int MaxRetries = 5;

    private static readonly Random Rand = new();

    private static Dictionary<string, string> Headers() => new()
    {
        ["Content-Type"] = "application/json",
        ["Accept"] = "application/json",
        ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    };

    private static string RandomName(int length = 12)
    {
        const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    private static string RandomPassword(int length = 16)
    {
        const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        var sb = new StringBuilder();
        for (var i = 0; i < length; i++) sb.Append(chars[Rand.Next(chars.Length)]);
        return sb.ToString();
    }

    /// <summary>计算 digest（字节串）二进制表示的前导零位数</summary>
    private static int LeadingZeroBits(byte[] digest)
    {
        var bits = 0;
        foreach (var b in digest)
        {
            if (b == 0) { bits += 8; continue; }
            for (var i = 7; i >= 0; i--)
            {
                if (((b >> i) & 1) != 0) return bits;
                bits++;
            }
            return bits;
        }
        return bits;
    }

    /// <summary>求解 PoW：SHA-256(address.lower+timestamp+nonce) 前导零位数 >= difficulty</summary>
    private static string SolvePow(string address, long timestamp, int difficulty)
    {
        var prefix = address.ToLowerInvariant().Trim() + timestamp;
        long nonce = 0;
        while (true)
        {
            var candidate = prefix + nonce;
            var digest = SHA256.HashData(Encoding.UTF8.GetBytes(candidate));
            if (LeadingZeroBits(digest) >= difficulty) return nonce.ToString();
            nonce++;
        }
    }

    private static string Sha256Hex(string input)
    {
        var digest = SHA256.HashData(Encoding.UTF8.GetBytes(input));
        var sb = new StringBuilder(digest.Length * 2);
        foreach (var b in digest) sb.Append(b.ToString("x2"));
        return sb.ToString();
    }

    private static List<string> FetchDomains()
    {
        var resp = Http.Get($"{BaseUrl}/domains", Headers());
        resp.EnsureSuccess();
        var data = Json.Parse(resp.Body) as JsonObject
            ?? throw new Exception("mail-td: 域名响应格式无效");
        if (data["domains"] is not JsonArray domains)
            throw new Exception("mail-td: 未获取到域名列表");

        var result = new List<string>();
        foreach (var node in domains)
        {
            if (node is not JsonObject item) continue;
            if (item["pro_only"]?.GetValue<bool>() == true) continue;
            var domain = Json.Str(item, "domain");
            if (domain.Length > 0) result.Add(domain);
        }
        if (result.Count == 0) throw new Exception("mail-td: 无可用域名");
        return result;
    }

    /// <summary>创建 mail.td 临时邮箱</summary>
    public static EmailInfo Generate()
    {
        var domains = FetchDomains();
        var domain = domains[Rand.Next(domains.Count)];
        var address = $"{RandomName()}@{domain}";
        var authKey = Sha256Hex(RandomPassword());

        var difficulty = InitialDifficulty;

        for (var attempt = 0; attempt < MaxRetries + 1; attempt++)
        {
            if (difficulty > MaxDifficulty)
                throw new Exception($"mail-td: PoW 难度 {difficulty} 超出上限 {MaxDifficulty}");

            var timestamp = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
            var nonce = SolvePow(address, timestamp, difficulty);

            var body = Json.Serialize(new Dictionary<string, object?>
            {
                ["address"] = address,
                ["auth_key"] = authKey,
                ["pow"] = new Dictionary<string, object?> { ["t"] = timestamp, ["n"] = nonce, ["d"] = difficulty },
            });
            var resp = Http.Post($"{BaseUrl}/accounts", body, "application/json", Headers());

            var data = Json.Parse(resp.Body) as JsonObject
                ?? throw new Exception("mail-td: 创建账户响应格式无效");

            // 服务端要求提升难度重试
            if (Json.Str(data, "status") == "retry")
            {
                var required = data["required_difficulty"]?.GetValue<int?>();
                if (required is not int req || req <= difficulty) difficulty += 1;
                else difficulty = req;
                continue;
            }

            resp.EnsureSuccess();

            var accountId = Json.Str(data, "id");
            var jwt = Json.Str(data, "token");
            var addr = Json.Str(data, "address");
            if (addr.Length == 0) addr = address;

            if (accountId.Length == 0 || jwt.Length == 0 || !addr.Contains('@'))
                throw new Exception($"mail-td: 创建账户失败，响应: {resp.Body}");

            var token = Json.Serialize(new Dictionary<string, object?> { ["jwt"] = jwt, ["id"] = accountId });
            return new EmailInfo("mail-td", addr, token);
        }

        throw new Exception("mail-td: PoW 重试次数已用尽，创建账户失败");
    }

    /// <summary>获取 mail.td 邮件列表</summary>
    public static List<Email> GetEmails(string? token, string email)
    {
        if (string.IsNullOrEmpty(token)) throw new Exception("mail-td: token 不能为空");
        var addr = (email ?? "").Trim();
        if (addr.Length == 0) throw new Exception("mail-td: 邮箱地址不能为空");

        var info = Json.Parse(token) as JsonObject ?? throw new Exception("mail-td: token 格式无效");
        var jwt = Json.Str(info, "jwt");
        var accountId = Json.Str(info, "id");
        if (jwt.Length == 0 || accountId.Length == 0) throw new Exception("mail-td: token 缺少 jwt 或 id");

        var headers = Headers();
        headers["Authorization"] = $"Bearer {jwt}";
        var resp = Http.Get($"{BaseUrl}/accounts/{WebUtility.UrlEncode(accountId)}/messages?page=1", headers);
        resp.EnsureSuccess();

        var data = Json.Parse(resp.Body) as JsonObject;
        var result = new List<Email>();
        if (data?["messages"] is not JsonArray messages) return result;

        foreach (var node in messages)
        {
            if (node is not JsonObject item) continue;
            // 发件人字段为对象 {"address","name"}，提取地址字符串
            var fromAddr = item["from"] is JsonObject sender ? Json.Str(sender, "address") : Json.Str(item, "from");
            var to = Json.Str(item, "to");
            if (to.Length == 0) to = addr;

            result.Add(Normalize.NormalizeEmail(new Dictionary<string, object?>
            {
                ["id"] = Json.Str(item, "id"),
                ["from"] = fromAddr,
                ["to"] = to,
                ["subject"] = Json.Str(item, "subject"),
                ["text"] = Json.Str(item, "text"),
                ["html"] = Json.Str(item, "html"),
                ["date"] = Json.Str(item, "created_at"),
            }, addr));
        }
        return result;
    }
}
