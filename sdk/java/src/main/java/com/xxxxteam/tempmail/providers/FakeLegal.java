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
import java.util.concurrent.ThreadLocalRandom;

/**
 * Fake Legal 渠道（imgui.de）。
 *
 * <p>域名别名：fake.legal 域 / imgui.de / pulsewebmenu.de</p>
 */
public final class FakeLegal {

    private static final String BASE = "https://imgui.de";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json, text/plain, */*");
        HEADERS.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        HEADERS.put("Cache-Control", "no-cache");
        HEADERS.put("DNT", "1");
        HEADERS.put("Pragma", "no-cache");
        HEADERS.put("Referer", "https://imgui.de/");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    }

    private FakeLegal() {
    }

    /**
     * 创建临时邮箱。
     *
     * @param channel 渠道标识
     * @param domain  指定域名，可为 null
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel, String domain) {
        List<String> domains = fetchDomains();
        if (domains.isEmpty()) {
            throw new RuntimeException("fake-legal: no domains");
        }
        String d = pickDomain(domains, domain);
        String username = randomUsername(12);

        HttpResult resp;
        if ("imgui.de".equals(d) || "pulsewebmenu.de".equals(d)) {
            // imgui-de / pulsewebmenu-de 用 POST 创建
            Map<String, Object> body = new LinkedHashMap<>();
            body.put("username", username);
            body.put("domain", d);
            resp = HttpClient.post(BASE + "/api/inbox/custom", Json.serialize(body), "application/json", HEADERS);
        } else {
            // fake-legal 用 GET 创建
            resp = HttpClient.get(BASE + "/api/inbox/new?domain=" + ProviderUtil.urlEncode(d), HEADERS);
        }

        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null || !data.has("success") || !data.get("success").getAsBoolean()) {
            throw new RuntimeException("fake-legal: create inbox failed");
        }
        String addr = Json.str(data, "address").trim();
        if (addr.isEmpty()) {
            throw new RuntimeException("fake-legal: missing address");
        }
        return new EmailInfo(channel, addr);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("fake-legal: empty email");
        }
        String e = email.trim();
        HttpResult resp = HttpClient.get(BASE + "/api/inbox/" + ProviderUtil.urlEncode(e), HEADERS);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null || !data.has("success") || !data.get("success").getAsBoolean()) {
            return new ArrayList<>();
        }
        JsonArray rows = Json.arr(data, "emails");
        if (rows == null) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement row : rows) {
            if (!row.isJsonObject()) {
                continue;
            }
            result.add(Normalizer.normalizeEmail(Json.toDict(row), e));
        }
        return result;
    }

    private static List<String> fetchDomains() {
        HttpResult resp = HttpClient.get(BASE + "/api/domains", HEADERS);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) {
            return new ArrayList<>();
        }
        JsonArray raw = Json.arr(data, "domains");
        if (raw == null) {
            return new ArrayList<>();
        }
        List<String> out = new ArrayList<>();
        for (JsonElement el : raw) {
            if (el.isJsonPrimitive()) {
                String d = el.getAsString().trim();
                if (!d.isEmpty()) {
                    out.add(d);
                }
            }
        }
        return out;
    }

    private static String pickDomain(List<String> domains, String preferred) {
        if (preferred != null && !preferred.isBlank()) {
            String p = preferred.trim().toLowerCase();
            for (String d : domains) {
                if (d.toLowerCase().equals(p)) {
                    return d;
                }
            }
            throw new IllegalArgumentException("fake-legal: domain not available: " + p);
        }
        return domains.get(ThreadLocalRandom.current().nextInt(domains.size()));
    }

    private static String randomUsername(int length) {
        String chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        StringBuilder sb = new StringBuilder(length);
        ThreadLocalRandom r = ThreadLocalRandom.current();
        for (int i = 0; i < length; i++) {
            sb.append(chars.charAt(r.nextInt(chars.length())));
        }
        return sb.toString();
    }
}
