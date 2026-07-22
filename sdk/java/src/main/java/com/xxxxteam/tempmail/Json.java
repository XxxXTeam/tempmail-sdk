package com.xxxxteam.tempmail;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.google.gson.JsonPrimitive;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * JSON 辅助工具，封装 gson。
 *
 * <p>provider 用 {@link #parse} 解析响应，用 {@link #toDict} / {@link #toRaw}
 * 把节点转成 {@link Normalizer} 需要的字典形态。</p>
 */
public final class Json {

    private static final Gson GSON = new Gson();

    private Json() {
    }

    /**
     * 解析 JSON 文本为 {@link JsonElement}（失败返回 null）。
     *
     * @param text JSON 文本
     * @return 解析结果，失败返回 null
     */
    public static JsonElement parse(String text) {
        if (text == null || text.isBlank()) {
            return null;
        }
        try {
            return JsonParser.parseString(text);
        } catch (RuntimeException e) {
            return null;
        }
    }

    /**
     * 解析为 JSON 对象，非对象返回 null。
     *
     * @param text JSON 文本
     * @return 对象节点，失败或非对象返回 null
     */
    public static JsonObject parseObject(String text) {
        JsonElement el = parse(text);
        return el != null && el.isJsonObject() ? el.getAsJsonObject() : null;
    }

    /**
     * 序列化任意对象为 JSON 文本。
     *
     * @param value 待序列化对象
     * @return JSON 文本
     */
    public static String serialize(Object value) {
        return GSON.toJson(value);
    }

    /**
     * 读取对象节点的字符串字段，缺失返回空串。
     *
     * @param node 对象节点
     * @param key  字段名
     * @return 字段字符串值，缺失返回空串
     */
    public static String str(JsonElement node, String key) {
        if (node != null && node.isJsonObject()) {
            JsonObject obj = node.getAsJsonObject();
            if (obj.has(key) && !obj.get(key).isJsonNull()) {
                return nodeToString(obj.get(key));
            }
        }
        return "";
    }

    /**
     * 把 {@link JsonElement} 转为标量字符串（对象/数组返回原始 JSON）。
     *
     * @param node 节点
     * @return 标量字符串
     */
    public static String nodeToString(JsonElement node) {
        if (node == null || node.isJsonNull()) {
            return "";
        }
        if (node.isJsonPrimitive()) {
            JsonPrimitive p = node.getAsJsonPrimitive();
            if (p.isString()) {
                return p.getAsString();
            }
            if (p.isBoolean()) {
                return p.getAsBoolean() ? "true" : "false";
            }
            return p.getAsString();
        }
        return node.toString();
    }

    /**
     * 将 {@link JsonElement} 递归转换为原生对象（对象转 Map，数组转 List，
     * 标量转 String/Double/Boolean/Long），供 {@link Normalizer} 消费。
     *
     * @param node 节点
     * @return 原生对象，可能为 null
     */
    public static Object toRaw(JsonElement node) {
        if (node == null || node.isJsonNull()) {
            return null;
        }
        if (node.isJsonObject()) {
            Map<String, Object> dict = new LinkedHashMap<>();
            for (Map.Entry<String, JsonElement> kv : node.getAsJsonObject().entrySet()) {
                dict.put(kv.getKey(), toRaw(kv.getValue()));
            }
            return dict;
        }
        if (node.isJsonArray()) {
            List<Object> list = new ArrayList<>();
            for (JsonElement item : node.getAsJsonArray()) {
                list.add(toRaw(item));
            }
            return list;
        }
        JsonPrimitive p = node.getAsJsonPrimitive();
        if (p.isBoolean()) {
            return p.getAsBoolean();
        }
        if (p.isNumber()) {
            double d = p.getAsDouble();
            if (d == Math.floor(d) && !Double.isInfinite(d)) {
                return (long) d;
            }
            return d;
        }
        return p.getAsString();
    }

    /**
     * 把 {@link JsonElement} 转为字典（非对象返回空字典）。
     *
     * @param node 节点
     * @return 字典
     */
    @SuppressWarnings("unchecked")
    public static Map<String, Object> toDict(JsonElement node) {
        Object raw = toRaw(node);
        return raw instanceof Map ? (Map<String, Object>) raw : new LinkedHashMap<>();
    }

    /**
     * 从对象节点取子数组（缺失或非数组返回 null）。
     *
     * @param node 对象节点
     * @param key  字段名
     * @return 数组节点，可能为 null
     */
    public static JsonArray arr(JsonElement node, String key) {
        if (node != null && node.isJsonObject()) {
            JsonObject obj = node.getAsJsonObject();
            if (obj.has(key) && obj.get(key).isJsonArray()) {
                return obj.getAsJsonArray(key);
            }
        }
        return null;
    }
}
