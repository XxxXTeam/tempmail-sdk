package com.xxxxteam.tempmail.providers;

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
 * Maildrop 渠道 — https://maildrop.cx
 *
 * <p>GET /api/suffixes.php 获取域名，GET /api/emails.php 获取邮件。</p>
 */
public final class Maildrop {

    private static final String BASE = "https://maildrop.cx";
    private static final String EXCLUDED = "transformer.edu.kg";

    private Maildrop() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json, text/plain, */*");
        h.put("Referer", "https://maildrop.cx/zh-cn/app");
        h.put("User-Agent", "Mozilla/5.0");
        h.put("X-Requested-With", "XMLHttpRequest");
        return h;
    }

    /**
     * 创建 maildrop 临时邮箱。
     *
     * @param domain 首选域名，可为 null
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        HttpResult resp = HttpClient.get(BASE + "/api/suffixes.php", headers());
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonArray()) {
            throw new RuntimeException("maildrop: 域名响应无效");
        }

        List<String> suffixes = new ArrayList<>();
        for (JsonElement el : parsed.getAsJsonArray()) {
            if (el.isJsonPrimitive()) {
                String s = el.getAsString().trim();
                if (!s.isEmpty() && !s.equalsIgnoreCase(EXCLUDED)) {
                    suffixes.add(s);
                }
            }
        }
        if (suffixes.isEmpty()) throw new RuntimeException("maildrop: 无可用域名");

        String dom;
        if (domain != null && !domain.isBlank()) {
            String p = domain.trim().toLowerCase();
            dom = suffixes.stream().filter(s -> s.equalsIgnoreCase(p)).findFirst()
                    .orElseThrow(() -> new RuntimeException("maildrop: 域名不可用: " + p));
        } else {
            dom = suffixes.get(ThreadLocalRandom.current().nextInt(suffixes.size()));
        }

        String local = ProviderUtil.randomString(10);
        String email = local + "@" + dom;
        return new EmailInfo("maildrop", email, email, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token 邮箱地址
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String addr = (email != null && !email.isBlank()) ? email.trim()
                : (token != null ? token.trim() : "");
        if (addr.isEmpty()) throw new RuntimeException("maildrop: 地址为空");

        String url = BASE + "/api/emails.php?addr=" + ProviderUtil.urlEncode(addr)
                + "&page=1&limit=20";
        HttpResult resp = HttpClient.get(url, headers());
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();
        var rows = Json.arr(body, "emails");
        if (rows == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : rows) {
            if (!item.isJsonObject()) continue;
            JsonObject row = item.getAsJsonObject();
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", Json.str(row, "id"));
            flat.put("from", Json.str(row, "from_addr"));
            flat.put("to", addr);
            flat.put("subject", Json.str(row, "subject"));
            flat.put("text", Json.str(row, "description"));
            flat.put("date", Json.str(row, "date"));
            out.add(Normalizer.normalizeEmail(flat, addr));
        }
        return out;
    }
}
