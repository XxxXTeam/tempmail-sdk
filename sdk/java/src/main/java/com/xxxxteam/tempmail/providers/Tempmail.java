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
 * tempmail.ing 渠道实现。
 *
 * <p>API: https://api.tempmail.ing/api</p>
 */
public final class Tempmail {

    private static final String BASE_URL = "https://api.tempmail.ing/api";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36");
        HEADERS.put("Content-Type", "application/json");
        HEADERS.put("Referer", "https://tempmail.ing/");
        HEADERS.put("sec-ch-ua", "\"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"");
        HEADERS.put("sec-ch-ua-mobile", "?0");
        HEADERS.put("sec-ch-ua-platform", "\"Windows\"");
        HEADERS.put("DNT", "1");
    }

    private Tempmail() {
    }

    /**
     * 创建临时邮箱，支持自定义有效期（分钟）。
     *
     * @param duration 有效期分钟数
     * @return 邮箱信息
     */
    public static EmailInfo generate(int duration) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("duration", duration);
        String payload = Json.serialize(body);
        HttpResult resp = HttpClient.post(BASE_URL + "/generate", payload, "application/json", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonObject()) {
            throw new RuntimeException("tempmail: 无效的创建响应");
        }
        JsonObject root = data.getAsJsonObject();
        boolean success = root.has("success") && root.get("success").getAsBoolean();
        if (!success) {
            throw new RuntimeException("tempmail: 创建邮箱失败");
        }
        JsonObject emailObj = root.has("email") && root.get("email").isJsonObject()
                ? root.getAsJsonObject("email") : null;
        if (emailObj == null) {
            throw new RuntimeException("tempmail: 响应缺少 email 对象");
        }
        String address = Json.str(emailObj, "address");
        String expiresAt = Json.str(emailObj, "expiresAt");
        String createdAt = Json.str(emailObj, "createdAt");
        if (address.isEmpty()) {
            throw new RuntimeException("tempmail: 响应缺少邮箱地址");
        }
        return new EmailInfo("tempmail", address, null,
                expiresAt.isEmpty() ? null : parseLong(expiresAt), createdAt.isEmpty() ? null : createdAt);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        HttpResult resp = HttpClient.get(
                BASE_URL + "/emails/" + ProviderUtil.urlEncode(email), HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonObject()) {
            return new ArrayList<>();
        }
        JsonObject root = data.getAsJsonObject();
        boolean success = root.has("success") && root.get("success").getAsBoolean();
        if (!success) {
            throw new RuntimeException("tempmail: 获取邮件失败");
        }
        var emails = Json.arr(root, "emails");
        List<Email> result = new ArrayList<>();
        if (emails == null) {
            return result;
        }
        for (JsonElement raw : emails) {
            if (!raw.isJsonObject()) {
                continue;
            }
            result.add(Normalizer.normalizeEmail(flattenMessage(raw.getAsJsonObject(), email), email));
        }
        return result;
    }

    private static Map<String, Object> flattenMessage(JsonObject raw, String recipientEmail) {
        Map<String, Object> dict = new LinkedHashMap<>();
        dict.put("id", Json.str(raw, "id"));
        dict.put("from", ProviderUtil.firstNonEmpty(Json.str(raw, "from_address"), Json.str(raw, "from")));
        dict.put("to", recipientEmail);
        dict.put("subject", Json.str(raw, "subject"));
        dict.put("text", Json.str(raw, "text"));
        dict.put("html", ProviderUtil.firstNonEmpty(Json.str(raw, "content"), Json.str(raw, "html")));
        dict.put("date", ProviderUtil.firstNonEmpty(Json.str(raw, "received_at"), Json.str(raw, "date")));
        boolean isRead = raw.has("is_read") && raw.get("is_read").isJsonPrimitive()
                && (raw.get("is_read").getAsBoolean()
                || "1".equals(raw.get("is_read").getAsString()));
        dict.put("is_read", isRead);
        return dict;
    }

    private static Long parseLong(String s) {
        try {
            return Long.parseLong(s);
        } catch (NumberFormatException e) {
            return null;
        }
    }
}
