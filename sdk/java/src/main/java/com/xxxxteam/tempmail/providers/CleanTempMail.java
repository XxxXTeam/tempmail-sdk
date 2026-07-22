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

/**
 * CleanTempMail 渠道（cleantempmail.com）。
 */
public final class CleanTempMail {

    private static final String API_BASE = "https://cleantempmail.com/api";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0");
        String apiKey = System.getenv("CLEANTEMPMAIL_API_KEY");
        HEADERS.put("X-API-Key", (apiKey != null && !apiKey.isBlank()) ? apiKey.trim() : "ct-test");
    }

    private CleanTempMail() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(API_BASE + "/generate-email", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("cleantempmail: invalid generate-email response");
        }
        JsonElement dataEl = parsed.getAsJsonObject().get("data");
        String email = "";
        if (dataEl != null && dataEl.isJsonObject()) {
            email = ProviderUtil.firstNonEmpty(
                    Json.str(dataEl, "email"),
                    Json.str(dataEl, "mailbox"));
        }
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("cleantempmail: invalid generate-email response");
        }
        return new EmailInfo("cleantempmail", email);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("cleantempmail: empty email");
        }
        String address = email.trim();
        HttpResult resp = HttpClient.get(
                API_BASE + "/emails?email=" + ProviderUtil.urlEncode(address), HEADERS);
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
        JsonElement rowsEl = dataEl.getAsJsonObject().get("emails");
        if (rowsEl == null || !rowsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement item : rowsEl.getAsJsonArray()) {
            if (item.isJsonObject()) {
                result.add(Normalizer.normalizeEmail(Json.toDict(item), address));
            }
        }
        return result;
    }
}
