using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace XxxXTeam.TempMail;

/// <summary>
/// JSON 辅助工具，封装 System.Text.Json。
/// provider 用 Parse 解析响应，用 ToRaw 把节点转成 Normalize 需要的字典形态。
/// </summary>
public static class Json
{
    private static readonly JsonSerializerOptions WriteOpts = new()
    {
        Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    /// <summary>解析 JSON 文本为 JsonNode（失败返回 null）</summary>
    public static JsonNode? Parse(string text)
    {
        if (string.IsNullOrWhiteSpace(text)) return null;
        try { return JsonNode.Parse(text); }
        catch { return null; }
    }

    /// <summary>序列化任意对象为 JSON 文本</summary>
    public static string Serialize(object? value) => JsonSerializer.Serialize(value, WriteOpts);

    /// <summary>读取对象节点的字符串字段，缺失返回空串</summary>
    public static string Str(JsonNode? node, string key)
    {
        if (node is JsonObject obj && obj.TryGetPropertyValue(key, out var v) && v is not null)
            return NodeToString(v);
        return "";
    }

    /// <summary>把 JsonNode 转为标量字符串（对象/数组返回原始 JSON）</summary>
    public static string NodeToString(JsonNode? node)
    {
        if (node is null) return "";
        if (node is JsonValue val)
        {
            if (val.TryGetValue<string>(out var s)) return s;
            if (val.TryGetValue<bool>(out var b)) return b ? "true" : "false";
            if (val.TryGetValue<double>(out var d)) return d.ToString(CultureInfo.InvariantCulture);
            if (val.TryGetValue<long>(out var l)) return l.ToString(CultureInfo.InvariantCulture);
        }
        return node.ToJsonString();
    }

    /// <summary>
    /// 将 JsonNode 递归转换为 object?（对象 => Dictionary，数组 => List，
    /// 标量 => string/double/bool/long），供 Normalize 消费。
    /// </summary>
    public static object? ToRaw(JsonNode? node)
    {
        switch (node)
        {
            case null:
                return null;
            case JsonObject obj:
            {
                var dict = new Dictionary<string, object?>();
                foreach (var kv in obj) dict[kv.Key] = ToRaw(kv.Value);
                return dict;
            }
            case JsonArray arr:
            {
                var list = new List<object?>();
                foreach (var item in arr) list.Add(ToRaw(item));
                return list;
            }
            case JsonValue val:
            {
                if (val.TryGetValue<bool>(out var b)) return b;
                if (val.TryGetValue<long>(out var l)) return l;
                if (val.TryGetValue<double>(out var d)) return d;
                if (val.TryGetValue<string>(out var s)) return s;
                return val.ToString();
            }
            default:
                return null;
        }
    }

    /// <summary>把 JsonNode 转为 Dictionary（非对象返回空字典）</summary>
    public static Dictionary<string, object?> ToDict(JsonNode? node)
        => ToRaw(node) as Dictionary<string, object?> ?? new Dictionary<string, object?>();
}
