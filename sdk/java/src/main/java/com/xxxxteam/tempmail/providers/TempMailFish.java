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
 * TempMail Fish 渠道 — https://tempmail.fish
 * POST /emails/new-email 创建，GET /emails/emails?emailAddress={email} 收信。
 * token 存储 JSON: {"email":"...","authKey":"..."}
 */
public final class TempMailFish {

    private static final String API_BASE = "https://api.tempmail.fish";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
    }

    private TempMailFish() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 JSON 字符串 {"email":"...","authKey":"..."}
     */
    public static EmailInfo generate() {
        Map<String, String> h = new LinkedHashMap<>(HEADERS);
        h.put("Content-Type", "application/json");
        HttpResult resp = HttpClient.post(API_BASE + "/emails/new-email", null, null, h);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String email = Json.str(data, "email").trim();
        String authKey = Json.str(data, "authKey").trim();
        if (email.isEmpty() || !email.contains("@") || authKey.isEmpty()) {
            throw new RuntimeException("tempmail-fish: 创建邮箱响应无效");
        }
        Map<String, Object> tokenMap = new LinkedHashMap<>();
        tokenMap.put("email", email);
        tokenMap.put("authKey", authKey);
        String token = Json.serialize(tokenMap);
        return new EmailInfo("tempmail-fish", email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 格式认证信息
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) {
            throw new RuntimeException("tempmail-fish: token 为空");
        }
        JsonElement tokenData = Json.parse(token);
        if (tokenData == null || !tokenData.isJsonObject()) {
            throw new RuntimeException("tempmail-fish: token 格式无效");
        }
        String address = Json.str(tokenData, "email").trim();
        String authKey = Json.str(tokenData, "authKey").trim();
        if (address.isEmpty() && email != null && !email.isBlank()) {
            address = email.trim();
        }
        if (address.isEmpty() || authKey.isEmpty()) {
            throw new RuntimeException("tempmail-fish: token 缺少 email 或 authKey");
        }
        Map<String, String> h = new LinkedHashMap<>(HEADERS);
        h.put("Authorization", authKey);
        HttpResult resp = HttpClient.get(
                API_BASE + "/emails/emails?emailAddress=" + ProviderUtil.urlEncode(address), h);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        JsonArray rows = null;
        if (parsed != null && parsed.isJsonArray()) {
            rows = parsed.getAsJsonArray();
        } else if (parsed != null && parsed.isJsonObject()) {
            rows = Json.arr(parsed, "emails");
        }
        if (rows == null) {
            return result;
        }
        for (JsonElement item : rows) {
            if (item == null || !item.isJsonObject()) {
                continue;
            }
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("from", Json.str(item, "from"));
            raw.put("to", Json.str(item, "to").isEmpty() ? address : Json.str(item, "to"));
            raw.put("subject", Json.str(item, "subject"));
            raw.put("text", Json.str(item, "textBody"));
            raw.put("html", Json.str(item, "htmlBody"));
            raw.put("date", Json.str(item, "timestamp"));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }
}
