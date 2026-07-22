package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
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
 * Mail10s 渠道（mail10s.com）。
 */
public final class Mail10s {

    private static final String BASE_URL = "https://mail10s.com";
    private static final String DOMAIN = "mail10s.com";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0");
    }

    private Mail10s() {
    }

    /**
     * 创建临时邮箱（本地随机生成）。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String email = ProviderUtil.randomLocalSdk() + "@" + DOMAIN;
        return new EmailInfo("mail10s", email, email, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("mail10s: empty email");
        }
        String address = email.trim();
        HttpResult resp = HttpClient.get(
                BASE_URL + "/api/emails/" + ProviderUtil.urlEncode(address) + "/inbox", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonObject()) {
            return result;
        }
        JsonElement dataEl = parsed.getAsJsonObject().get("data");
        if (dataEl == null || !dataEl.isJsonObject()) {
            return result;
        }
        JsonElement msgsEl = dataEl.getAsJsonObject().get("messages");
        if (msgsEl == null || !msgsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement item : msgsEl.getAsJsonArray()) {
            if (!item.isJsonObject()) {
                continue;
            }
            Map<String, Object> raw = Json.toDict(item);
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", raw.getOrDefault("id", ""));
            flat.put("from", raw.getOrDefault("sender", ""));
            flat.put("to", address);
            flat.put("subject", raw.getOrDefault("subject", ""));
            flat.put("text", raw.getOrDefault("body_text", ""));
            flat.put("html", raw.getOrDefault("body_html", ""));
            flat.put("date", raw.getOrDefault("received_at", ""));
            flat.put("attachments", raw.getOrDefault("attachments", new ArrayList<>()));
            result.add(Normalizer.normalizeEmail(flat, address));
        }
        return result;
    }
}
