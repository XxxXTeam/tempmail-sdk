using System;
using System.Collections.Generic;
using System.Text;

namespace XxxXTeam.TempMail.Providers;

/// <summary>
/// provider 公共 HTTP/Cookie 辅助工具。
/// 集中处理 Set-Cookie 解析、Cookie 头合并等复杂渠道通用逻辑，避免各 provider 重复实现。
/// </summary>
internal static class ProviderHttpUtil
{
    /// <summary>
    /// 从响应的 Set-Cookie 列表中提取指定名称的 cookie 值（不含名称）。
    /// 未找到返回空串。
    /// </summary>
    public static string ExtractCookieValue(HttpResult resp, string name)
    {
        foreach (var raw in resp.SetCookies)
        {
            var nv = FirstPair(raw);
            var eq = nv.IndexOf('=');
            if (eq <= 0) continue;
            if (string.Equals(nv[..eq].Trim(), name, StringComparison.OrdinalIgnoreCase))
                return nv[(eq + 1)..].Trim();
        }
        return "";
    }

    /// <summary>
    /// 将响应中的所有 Set-Cookie 解析为 "k=v; k=v" 形式的 Cookie 请求头（按名称排序）。
    /// </summary>
    public static string CookieHeaderFrom(HttpResult resp)
    {
        var map = new SortedDictionary<string, string>(StringComparer.Ordinal);
        foreach (var raw in resp.SetCookies)
        {
            var nv = FirstPair(raw);
            var eq = nv.IndexOf('=');
            if (eq <= 0) continue;
            var k = nv[..eq].Trim();
            var v = nv[(eq + 1)..].Trim();
            if (k.Length > 0) map[k] = v;
        }
        return Join(map);
    }

    /// <summary>
    /// 合并已有 Cookie 头字符串与响应新的 Set-Cookie，返回合并后的 Cookie 头（按名称排序）。
    /// </summary>
    public static string MergeCookies(string prev, HttpResult resp)
    {
        var map = ParseCookieMap(prev);
        foreach (var raw in resp.SetCookies)
        {
            var nv = FirstPair(raw);
            var eq = nv.IndexOf('=');
            if (eq <= 0) continue;
            var k = nv[..eq].Trim();
            var v = nv[(eq + 1)..].Trim();
            if (k.Length > 0) map[k] = v;
        }
        var sorted = new SortedDictionary<string, string>(map, StringComparer.Ordinal);
        return Join(sorted);
    }

    /// <summary>解析 "k=v; k=v" Cookie 头字符串为字典</summary>
    public static Dictionary<string, string> ParseCookieMap(string header)
    {
        var m = new Dictionary<string, string>(StringComparer.Ordinal);
        if (string.IsNullOrEmpty(header)) return m;
        foreach (var part in header.Split(';'))
        {
            var p = part.Trim();
            if (p.Length == 0) continue;
            var eq = p.IndexOf('=');
            if (eq <= 0) continue;
            var k = p[..eq].Trim();
            var v = p[(eq + 1)..].Trim();
            if (k.Length > 0) m[k] = v;
        }
        return m;
    }

    /// <summary>取 Set-Cookie 原始串中第一段（"name=value"，去掉 Path/Expires 等属性）</summary>
    private static string FirstPair(string setCookie)
    {
        var semi = setCookie.IndexOf(';');
        return (semi < 0 ? setCookie : setCookie[..semi]).Trim();
    }

    private static string Join(IDictionary<string, string> map)
    {
        var sb = new StringBuilder();
        foreach (var kv in map)
        {
            if (sb.Length > 0) sb.Append("; ");
            sb.Append(kv.Key).Append('=').Append(kv.Value);
        }
        return sb.ToString();
    }
}
