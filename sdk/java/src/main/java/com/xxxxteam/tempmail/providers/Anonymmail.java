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
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Anonymmail 渠道（anonymmail.net）。
 * POST JSON API，需要 cookie session。
 */
public final class Anonymmail {

    private static final String BASE = "https://anonymmail.net";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "*/*");
        HEADERS.put("Content-Type", "application/x-www-form-urlencoded");
        HEADERS.put("Referer", "https://anonymmail.net/");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    }

    private Anonymmail() {
    }

    /**
     * 获取可用域名列表。
     *
     * @return 域名列表
     */
    private static List<String> fetchDomains() {
        HttpResult resp = HttpClient.post(BASE + "/api/getDomains", null, "application/x-www-form-urlencoded", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        List<String> domains = new ArrayList<>();
        if (data != null && data.isJsonArray()) {
            for (JsonElement item : data.getAsJsonArray()) {
                if (item.isJsonObject()) {
                    String domain = Json.str(item, "domain").trim();
                    if (!domain.isEmpty()) {
                        domains.add(domain);
                    }
                }
            }
        }
        return domains;
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        List<String> domains = fetchDomains();
        if (domains.isEmpty()) {
            throw new RuntimeException("anonymmail: no domains available");
        }
        String domain = domains.get(java.util.concurrent.ThreadLocalRandom.current().nextInt(domains.size()));
        String username = ProviderUtil.randomString(10);
        String email = username + "@" + domain;

        // POST 创建邮箱
        HttpResult resp = HttpClient.post(
                BASE + "/api/create", "email=" + ProviderUtil.urlEncode(email),
                "application/x-www-form-urlencoded", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonObject()) {
            throw new RuntimeException("anonymmail: create inbox failed");
        }
        JsonObject obj = data.getAsJsonObject();
        if (!obj.has("success") || !obj.get("success").getAsBoolean()) {
            throw new RuntimeException("anonymmail: create inbox failed");
        }
        String addr = Json.str(data, "email").trim();
        if (addr.isEmpty()) {
            throw new RuntimeException("anonymmail: missing email in response");
        }
        return new EmailInfo("anonymmail", addr);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("anonymmail: empty email");
        }
        String e = email.trim();
        HttpResult resp = HttpClient.post(
                BASE + "/api/get", "email=" + ProviderUtil.urlEncode(e),
                "application/x-www-form-urlencoded", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (data == null || !data.isJsonObject()) {
            return result;
        }
        // 响应格式: {"email@domain": {"created_at": "...", "emails": [...]}}
        JsonObject root = data.getAsJsonObject();
        if (!root.has(e) || !root.get(e).isJsonObject()) {
            return result;
        }
        JsonObject inbox = root.getAsJsonObject(e);
        JsonArray rows = Json.arr(inbox, "emails");
        if (rows == null) {
            return result;
        }
        for (JsonElement raw : rows) {
            if (!raw.isJsonObject()) {
                continue;
            }
            Map<String, Object> normalized = Json.toDict(raw);
            // 将 token 字段映射为 id
            if (normalized.containsKey("token") && !normalized.containsKey("id")) {
                normalized.put("id", normalized.remove("token"));
            }
            // 将 body 字段映射为 html
            if (normalized.containsKey("body") && !normalized.containsKey("html")) {
                normalized.put("html", normalized.get("body"));
                normalized.remove("body");
            }
            result.add(Normalizer.normalizeEmail(normalized, e));
        }
        return result;
    }
}
