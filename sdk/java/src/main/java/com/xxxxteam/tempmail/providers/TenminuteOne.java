package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * 10minutemail.one 渠道实现。
 *
 * <p>SSR __NUXT_DATA__ 中的 mailServiceToken（JWT）+ 页面 emailDomains；
 * 收信 GET web.10minutemail.one/api/v1/mailbox/{email}</p>
 *
 * <p>域名别名：xghff.com / oqqaj.com / psovv.com / dbwot.com / ygwpr.com / imxwe.com</p>
 */
public final class TenminuteOne {

    private static final String SITE_ORIGIN = "https://10minutemail.one";
    private static final String API_BASE = "https://web.10minutemail.one/api/v1";
    private static final String[] KNOWN_DOMAINS = {
            "xghff.com", "oqqaj.com", "psovv.com", "dbwot.com", "ygwpr.com", "imxwe.com"
    };
    private static final String DEFAULT_UA =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                    + "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

    private static final Pattern NUXT_DATA_RE = Pattern.compile(
            "<script[^>]*\\bid=\"__NUXT_DATA__\"[^>]*>([\\s\\S]*?)</script>", Pattern.CASE_INSENSITIVE);
    private static final Pattern JWT_RE = Pattern.compile(
            "^[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+$");

    private TenminuteOne() {
    }

    /**
     * 创建临时邮箱。
     *
     * @param channel 渠道标识
     * @param domain  指定域名，可为 null
     * @return 邮箱信息（token 存储 JWT）
     */
    public static EmailInfo generate(String channel, String domain) {
        String pageUrl = SITE_ORIGIN + "/zh";
        Map<String, String> pageHeaders = new LinkedHashMap<>();
        pageHeaders.put("User-Agent", DEFAULT_UA);
        pageHeaders.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        pageHeaders.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        pageHeaders.put("Cache-Control", "no-cache");
        pageHeaders.put("Pragma", "no-cache");
        pageHeaders.put("DNT", "1");
        pageHeaders.put("Referer", pageUrl);

        HttpResult r = HttpClient.get(pageUrl, pageHeaders);
        r.ensureSuccess();
        String html = r.getBody();

        // 解析 __NUXT_DATA__ 获取 JWT token
        String token = parseMailServiceToken(html);

        // 解析可用域名
        List<String> domains = parseEmailDomains(html);
        if (domains.isEmpty()) {
            for (String d : KNOWN_DOMAINS) {
                domains.add(d);
            }
        }

        // 选择域名
        String mailDomain;
        String domHint = (domain != null && domain.contains(".")) ? domain.trim() : null;
        if (domHint != null) {
            String h = domHint.toLowerCase();
            mailDomain = null;
            for (String d : domains) {
                if (d.toLowerCase().equals(h)) {
                    mailDomain = d;
                    break;
                }
            }
            if (mailDomain == null) {
                mailDomain = domains.get(ThreadLocalRandom.current().nextInt(domains.size()));
            }
        } else {
            mailDomain = domains.get(ThreadLocalRandom.current().nextInt(domains.size()));
        }

        // 生成随机本地名
        String local = ProviderUtil.randomString(10);
        String address = local + "@" + mailDomain;

        // 解析 JWT 过期时间
        Long expMs = jwtExpMs(token);

        return new EmailInfo(channel, address, token, expMs, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token JWT 令牌
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String url = API_BASE + "/mailbox/" + encMailboxEmail(email);
        HttpResult r = HttpClient.get(url, apiHeaders(token));
        r.ensureSuccess();
        JsonElement data = Json.parse(r.getBody());
        if (data == null || !data.isJsonArray()) {
            throw new RuntimeException("10minute-one: invalid inbox JSON");
        }

        List<Email> result = new ArrayList<>();
        for (JsonElement el : data.getAsJsonArray()) {
            if (!el.isJsonObject()) {
                continue;
            }
            JsonObject raw = el.getAsJsonObject();
            Map<String, Object> row = Json.toDict(raw);

            // 如果正文为空则尝试拉取详情
            if (needsDetail(raw)) {
                String mid = Json.str(raw, "id");
                if (!mid.isEmpty()) {
                    try {
                        String du = API_BASE + "/mailbox/" + encMailboxEmail(email)
                                + "/" + ProviderUtil.urlEncode(mid);
                        HttpResult d = HttpClient.get(du, apiHeaders(token));
                        if (d.isOk()) {
                            JsonObject detail = Json.parseObject(d.getBody());
                            if (detail != null) {
                                Map<String, Object> detailMap = Json.toDict(detail);
                                for (Map.Entry<String, Object> entry : detailMap.entrySet()) {
                                    row.putIfAbsent(entry.getKey(), entry.getValue());
                                }
                            }
                        }
                    } catch (RuntimeException ignored) {
                        // 回退到列表摘要
                    }
                }
            }
            result.add(Normalizer.normalizeEmail(row, email));
        }
        return result;
    }

    private static boolean needsDetail(JsonObject m) {
        String id = Json.str(m, "id").trim();
        if (id.isEmpty()) {
            return false;
        }
        String body = ProviderUtil.firstNonEmpty(
                Json.str(m, "text"), Json.str(m, "body"),
                Json.str(m, "html"), Json.str(m, "mail_text"));
        return body.isEmpty();
    }

    private static String encMailboxEmail(String email) {
        if (email.contains("@")) {
            String[] parts = email.split("@", 2);
            return ProviderUtil.urlEncode(parts[0]) + "%40" + ProviderUtil.urlEncode(parts[1]);
        }
        return ProviderUtil.urlEncode(email);
    }

    private static Map<String, String> apiHeaders(String token) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "*/*");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        h.put("Authorization", "Bearer " + token);
        h.put("Cache-Control", "no-cache");
        h.put("Content-Type", "application/json");
        h.put("DNT", "1");
        h.put("Origin", SITE_ORIGIN);
        h.put("Pragma", "no-cache");
        h.put("Referer", SITE_ORIGIN + "/");
        h.put("User-Agent", DEFAULT_UA);
        h.put("X-Request-ID", ProviderUtil.randomString(32));
        h.put("X-Timestamp", String.valueOf(System.currentTimeMillis() / 1000));
        return h;
    }

    private static String parseMailServiceToken(String html) {
        Matcher m = NUXT_DATA_RE.matcher(html);
        if (!m.find()) {
            throw new RuntimeException("10minute-one: __NUXT_DATA__ not found");
        }
        String nuxtJson = m.group(1).trim();
        JsonElement parsed = Json.parse(nuxtJson);
        if (parsed == null || !parsed.isJsonArray()) {
            throw new RuntimeException("10minute-one: invalid __NUXT_DATA__");
        }
        JsonArray arr = parsed.getAsJsonArray();

        // 先查找包含 mailServiceToken 字段的对象
        for (int i = 0; i < Math.min(arr.size(), 48); i++) {
            JsonElement el = arr.get(i);
            if (el.isJsonObject() && el.getAsJsonObject().has("mailServiceToken")) {
                JsonElement tokenRef = el.getAsJsonObject().get("mailServiceToken");
                String resolved = resolveRef(arr, tokenRef);
                if (resolved != null && JWT_RE.matcher(resolved).matches()) {
                    return resolved;
                }
            }
        }
        // 扩大搜索
        for (JsonElement el : arr) {
            if (el.isJsonObject() && el.getAsJsonObject().has("mailServiceToken")) {
                JsonElement tokenRef = el.getAsJsonObject().get("mailServiceToken");
                String resolved = resolveRef(arr, tokenRef);
                if (resolved != null && JWT_RE.matcher(resolved).matches()) {
                    return resolved;
                }
            }
        }
        // 最后尝试直接找 JWT 字符串
        for (JsonElement el : arr) {
            if (el.isJsonPrimitive() && el.getAsJsonPrimitive().isString()) {
                String s = el.getAsString();
                if (JWT_RE.matcher(s).matches()) {
                    return s;
                }
            }
        }
        throw new RuntimeException("10minute-one: mailServiceToken not found");
    }

    private static String resolveRef(JsonArray arr, JsonElement value) {
        return resolveRef(arr, value, 0);
    }

    private static String resolveRef(JsonArray arr, JsonElement value, int depth) {
        if (depth > 64) {
            return value != null && value.isJsonPrimitive() ? value.getAsString() : null;
        }
        if (value == null || value.isJsonNull()) {
            return null;
        }
        if (value.isJsonPrimitive()) {
            if (value.getAsJsonPrimitive().isString()) {
                return value.getAsString();
            }
            if (value.getAsJsonPrimitive().isNumber()) {
                int idx = value.getAsInt();
                if (idx >= 0 && idx < arr.size()) {
                    return resolveRef(arr, arr.get(idx), depth + 1);
                }
            }
        }
        return null;
    }

    private static List<String> parseEmailDomains(String html) {
        List<String> result = new ArrayList<>();
        // 尝试从 HTML 中解析 emailDomains 字段
        String key = "emailDomains:\"";
        int start = html.indexOf(key);
        if (start < 0) {
            return result;
        }
        int sliceStart = start + key.length();
        if (sliceStart >= html.length() || html.charAt(sliceStart) != '[') {
            return result;
        }
        int depth = 0;
        int j = sliceStart;
        while (j < html.length()) {
            char c = html.charAt(j);
            if (c == '[') {
                depth++;
            } else if (c == ']') {
                depth--;
                if (depth == 0) {
                    j++;
                    break;
                }
            }
            j++;
        }
        String raw = html.substring(sliceStart, j).replace("\\\"", "\"");
        try {
            JsonElement parsed = Json.parse(raw);
            if (parsed != null && parsed.isJsonArray()) {
                for (JsonElement el : parsed.getAsJsonArray()) {
                    if (el.isJsonPrimitive()) {
                        result.add(el.getAsString());
                    }
                }
            }
        } catch (RuntimeException ignored) {
            // 解析失败返回空列表
        }
        return result;
    }

    private static Long jwtExpMs(String token) {
        String[] parts = token.split("\\.");
        if (parts.length < 2) {
            return null;
        }
        try {
            String payload = parts[1];
            // 补齐 Base64 padding
            int pad = (4 - payload.length() % 4) % 4;
            payload = payload + "=".repeat(pad);
            payload = payload.replace('-', '+').replace('_', '/');
            byte[] decoded = Base64.getDecoder().decode(payload);
            JsonObject obj = Json.parseObject(new String(decoded, java.nio.charset.StandardCharsets.UTF_8));
            if (obj != null && obj.has("exp") && obj.get("exp").isJsonPrimitive()) {
                long exp = obj.get("exp").getAsLong();
                return exp * 1000;
            }
        } catch (RuntimeException ignored) {
            // JWT 解析失败
        }
        return null;
    }
}
