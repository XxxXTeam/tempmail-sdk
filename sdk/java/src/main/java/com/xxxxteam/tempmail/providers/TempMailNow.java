package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Temp-Mail.Now 渠道 — https://temp-mail.now
 * Cookie 会话认证，POST /change_email 创建邮箱，GET /fetch_emails 收信。
 */
public final class TempMailNow {

    private static final String BASE_URL = "https://temp-mail.now";
    private static final Map<String, String> PAGE_HEADERS = new LinkedHashMap<>();
    private static final Map<String, String> API_HEADERS = new LinkedHashMap<>();

    static {
        PAGE_HEADERS.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        PAGE_HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        API_HEADERS.put("Accept", "application/json, text/javascript, */*; q=0.01");
        API_HEADERS.put("X-Requested-With", "XMLHttpRequest");
        API_HEADERS.put("Referer", BASE_URL + "/en/");
        API_HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    }

    private TempMailNow() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 session cookie 字符串
     */
    public static EmailInfo generate() {
        // 获取 session cookie
        HttpResult resp = HttpClient.get(BASE_URL + "/en/", PAGE_HEADERS);
        resp.ensureSuccess();
        String cookie = ProviderUtil.extractAllCookies(resp);
        if (cookie.isEmpty()) {
            throw new RuntimeException("temp-mail-now: 无法获取 session cookie");
        }
        // 创建新邮箱
        Map<String, String> h = new LinkedHashMap<>(API_HEADERS);
        h.put("Cookie", cookie);
        HttpResult resp2 = HttpClient.post(BASE_URL + "/change_email", null, null, h);
        resp2.ensureSuccess();
        // 合并新 cookie
        String newCookies = ProviderUtil.extractAllCookies(resp2);
        if (!newCookies.isEmpty()) {
            cookie = ProviderUtil.mergeCookieStrings(cookie, newCookies);
        }
        JsonElement data = Json.parse(resp2.getBody());
        String address = Json.str(data, "new_email").trim();
        if (address.isEmpty() || !address.contains("@")) {
            throw new RuntimeException("temp-mail-now: 创建邮箱失败");
        }
        return new EmailInfo("temp-mail-now", address, cookie, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token session cookie
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String cookie = (token != null ? token : "").trim();
        String address = (email != null ? email : "").trim();
        if (cookie.isEmpty()) {
            throw new RuntimeException("temp-mail-now: session cookie 为空");
        }
        if (address.isEmpty()) {
            throw new RuntimeException("temp-mail-now: 邮箱地址为空");
        }
        Map<String, String> h = new LinkedHashMap<>(API_HEADERS);
        h.put("Cookie", cookie);
        HttpResult resp = HttpClient.get(BASE_URL + "/fetch_emails", h);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonArray emails = Json.arr(data, "emails");
        if (emails == null) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : emails) {
            if (item == null || !item.isJsonObject()) {
                continue;
            }
            Map<String, Object> raw = Json.toDict(item);
            raw.putIfAbsent("to", address);
            raw.putIfAbsent("from", raw.getOrDefault("from_address", raw.getOrDefault("sender", "")));
            raw.putIfAbsent("text", raw.getOrDefault("body_text", ""));
            raw.putIfAbsent("html", raw.getOrDefault("body_html", ""));
            raw.putIfAbsent("date", raw.getOrDefault("received_at", ""));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }
}
