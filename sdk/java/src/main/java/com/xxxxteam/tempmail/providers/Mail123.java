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
 * Mail123 渠道（mail123.fr）。
 */
public final class Mail123 {

    private static final String API_BASE = "https://mail123.fr/api/v1";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0");
    }

    private Mail123() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(API_BASE + "/mailbox/new", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("mail123: invalid mailbox response");
        }
        String email = Json.str(parsed, "address").trim();
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("mail123: invalid mailbox response");
        }
        Long expiresAt = null;
        String expiresInDays = Json.str(parsed, "expires_in_days").trim();
        if (!expiresInDays.isEmpty()) {
            try {
                double days = Double.parseDouble(expiresInDays);
                if (days > 0) {
                    expiresAt = (long) ((System.currentTimeMillis() / 1000.0 + days * 86400) * 1000);
                }
            } catch (NumberFormatException ignored) {
            }
        }
        return new EmailInfo("mail123", email, email, expiresAt, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("mail123: empty email");
        }
        String address = email.trim();
        HttpResult resp = HttpClient.get(
                API_BASE + "/mailbox/" + ProviderUtil.urlEncode(address) + "/messages?limit=50", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonObject()) {
            return result;
        }
        JsonElement rowsEl = parsed.getAsJsonObject().get("messages");
        if (rowsEl == null || !rowsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement item : rowsEl.getAsJsonArray()) {
            if (!item.isJsonObject()) {
                continue;
            }
            Map<String, Object> row = Json.toDict(item);
            String messageId = row.get("id") != null ? row.get("id").toString().trim() : "";
            Map<String, Object> detail = messageId.isEmpty() ? null : fetchDetail(address, messageId);
            Map<String, Object> src = detail != null ? detail : row;
            Map<String, Object> flat = new LinkedHashMap<>(src);
            flat.put("id", src.getOrDefault("id", ""));
            flat.put("to", src.getOrDefault("to", address));
            flat.put("text", ProviderUtil.firstNonEmpty(
                    strVal(src, "text"), strVal(src, "preview")));
            flat.put("html", src.getOrDefault("html", ""));
            result.add(Normalizer.normalizeEmail(flat, address));
        }
        return result;
    }

    private static Map<String, Object> fetchDetail(String address, String messageId) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/mailbox/" + ProviderUtil.urlEncode(address)
                            + "/messages/" + ProviderUtil.urlEncode(messageId), HEADERS);
            if (resp.getStatusCode() < 200 || resp.getStatusCode() >= 300) {
                return null;
            }
            JsonElement parsed = Json.parse(resp.getBody());
            if (parsed != null && parsed.isJsonObject()) {
                JsonElement msgEl = parsed.getAsJsonObject().get("message");
                if (msgEl != null && msgEl.isJsonObject()) {
                    return Json.toDict(msgEl);
                }
            }
        } catch (RuntimeException ignored) {
        }
        return null;
    }

    private static String strVal(Map<String, Object> m, String key) {
        Object v = m.get(key);
        return v != null ? v.toString() : "";
    }
}
