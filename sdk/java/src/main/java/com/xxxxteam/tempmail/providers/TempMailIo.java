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
 * temp-mail.io 渠道（api.internal.temp-mail.io/api/v3）。
 */
public final class TempMailIo {

    private static final String BASE_URL = "https://api.internal.temp-mail.io/api/v3";

    private TempMailIo() {
    }

    private static Map<String, String> buildHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Application-Name", "web");
        h.put("Application-Version", "4.0.0");
        h.put("X-CORS-Header", "1");
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0");
        h.put("Origin", "https://temp-mail.io");
        h.put("Referer", "https://temp-mail.io/");
        return h;
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("min_name_length", 10);
        body.put("max_name_length", 10);
        String payload = Json.serialize(body);
        HttpResult resp = HttpClient.post(BASE_URL + "/email/new", payload, "application/json", buildHeaders());
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String email = Json.str(data, "email");
        String token = Json.str(data, "token");
        if (email.isEmpty() || token.isEmpty()) {
            throw new RuntimeException("temp-mail-io: invalid generate response");
        }
        return new EmailInfo("temp-mail-io", email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        HttpResult resp = HttpClient.get(
                BASE_URL + "/email/" + ProviderUtil.urlEncode(email) + "/messages", buildHeaders());
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonArray()) {
            return result;
        }
        for (JsonElement raw : parsed.getAsJsonArray()) {
            if (raw.isJsonObject()) {
                result.add(Normalizer.normalizeEmail(Json.toDict(raw), email));
            }
        }
        return result;
    }
}
