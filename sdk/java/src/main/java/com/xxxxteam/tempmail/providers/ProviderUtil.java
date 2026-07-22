package com.xxxxteam.tempmail.providers;

import com.xxxxteam.tempmail.HttpResult;

import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ThreadLocalRandom;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * provider 通用工具：随机串、URL 编码、Cookie 处理、非空择取等。
 */
final class ProviderUtil {

    private static final String CHARS = "abcdefghijklmnopqrstuvwxyz0123456789";

    /** CSRF meta 标签正则，预编译避免每次调用重编译。 */
    private static final Pattern CSRF_RE =
            Pattern.compile("<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"");

    /** 动态正则编译缓存，避免 {@link #regexExtract} 每次调用重复编译相同表达式。 */
    private static final ConcurrentHashMap<String, Pattern> PATTERN_CACHE = new ConcurrentHashMap<>();

    private ProviderUtil() {
    }

    /**
     * 生成指定长度的随机小写字母数字串。
     *
     * @param len 长度
     * @return 随机串
     */
    static String randomString(int len) {
        StringBuilder sb = new StringBuilder(len);
        ThreadLocalRandom r = ThreadLocalRandom.current();
        for (int i = 0; i < len; i++) {
            sb.append(CHARS.charAt(r.nextInt(CHARS.length())));
        }
        return sb.toString();
    }

    /**
     * 生成以 "sdk" 开头再拼接 16 位随机串的本地名。
     *
     * @return 本地名
     */
    static String randomLocalSdk() {
        return "sdk" + randomString(16);
    }

    /**
     * URL 编码。
     *
     * @param s 原字符串
     * @return 编码结果
     */
    static String urlEncode(String s) {
        return URLEncoder.encode(s != null ? s : "", StandardCharsets.UTF_8);
    }

    /**
     * 返回第一个非空白字符串，全空返回空串。
     *
     * @param values 候选值
     * @return 首个非空白值
     */
    static String firstNonEmpty(String... values) {
        for (String v : values) {
            if (v != null && !v.isBlank()) {
                return v;
            }
        }
        return "";
    }

    /**
     * 从 HttpResult 的 Set-Cookie 头中提取所有 cookie，拼接为 "k=v; k2=v2" 格式。
     *
     * @param resp HTTP 响应
     * @return cookie 字符串
     */
    static String extractAllCookies(HttpResult resp) {
        Map<String, String> map = new LinkedHashMap<>();
        for (String raw : resp.getSetCookies()) {
            String pair = raw.split(";", 2)[0].trim();
            int eq = pair.indexOf('=');
            if (eq > 0) {
                map.put(pair.substring(0, eq).trim(), pair.substring(eq + 1).trim());
            }
        }
        if (map.isEmpty()) {
            return "";
        }
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> e : map.entrySet()) {
            if (sb.length() > 0) sb.append("; ");
            sb.append(e.getKey()).append('=').append(e.getValue());
        }
        return sb.toString();
    }

    /**
     * 合并两个 cookie 字符串（新 cookie 覆盖旧的同名键）。
     *
     * @param existing 已有 cookie 串
     * @param newer    新 cookie 串
     * @return 合并后的 cookie 串
     */
    static String mergeCookieStrings(String existing, String newer) {
        Map<String, String> map = new LinkedHashMap<>();
        parseCookieString(existing, map);
        parseCookieString(newer, map);
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> e : map.entrySet()) {
            if (sb.length() > 0) sb.append("; ");
            sb.append(e.getKey()).append('=').append(e.getValue());
        }
        return sb.toString();
    }

    private static void parseCookieString(String cookieStr, Map<String, String> out) {
        if (cookieStr == null || cookieStr.isEmpty()) return;
        for (String part : cookieStr.split(";")) {
            String trimmed = part.trim();
            int eq = trimmed.indexOf('=');
            if (eq > 0) {
                out.put(trimmed.substring(0, eq).trim(), trimmed.substring(eq + 1).trim());
            }
        }
    }

    /**
     * 从 HTML 中用正则提取 CSRF token（meta name="csrf-token" content="xxx"）。
     *
     * @param html HTML 文本
     * @return CSRF token，未匹配返回空串
     */
    static String extractCsrfToken(String html) {
        if (html == null || html.isEmpty()) return "";
        Matcher m = CSRF_RE.matcher(html);
        return m.find() ? m.group(1) : "";
    }

    /**
     * 用指定正则从文本中提取第一个捕获组。
     *
     * @param text    源文本
     * @param pattern 正则表达式（含一个捕获组）
     * @return 匹配内容，未匹配返回空串
     */
    static String regexExtract(String text, String pattern) {
        if (text == null || text.isEmpty()) return "";
        Matcher m = PATTERN_CACHE.computeIfAbsent(pattern, Pattern::compile).matcher(text);
        return m.find() ? m.group(1) : "";
    }

    /**
     * Base64 编码。
     *
     * @param data 原始字节
     * @return base64 字符串
     */
    static String base64Encode(byte[] data) {
        return Base64.getEncoder().encodeToString(data);
    }

    /**
     * Base64 解码。
     *
     * @param encoded base64 字符串
     * @return 原始字节
     */
    static byte[] base64Decode(String encoded) {
        return Base64.getDecoder().decode(encoded);
    }
}
