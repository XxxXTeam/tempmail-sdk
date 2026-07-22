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

import java.time.Instant;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * MFFAC 渠道实现 — https://www.mffac.com/api
 */
public final class Mffac {

    private static final String BASE = "https://www.mffac.com/api";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();
    private static final Map<String, String> GET_HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        HEADERS.put("Content-Type", "application/json");
        HEADERS.put("Accept", "*/*");
        HEADERS.put("Origin", "https://www.mffac.com");
        HEADERS.put("Referer", "https://www.mffac.com/");
        HEADERS.put("sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"");
        HEADERS.put("sec-ch-ua-mobile", "?0");
        HEADERS.put("sec-ch-ua-platform", "\"Windows\"");
        HEADERS.put("sec-fetch-dest", "empty");
        HEADERS.put("sec-fetch-mode", "cors");
        HEADERS.put("sec-fetch-site", "same-origin");

        for (Map.Entry<String, String> kv : HEADERS.entrySet()) {
            if (!"Content-Type".equals(kv.getKey())) {
                GET_HEADERS.put(kv.getKey(), kv.getValue());
            }
        }
    }

    private Mffac() {
    }

    /**
     * 将 receivedAt 秒级时间戳转为 ISO 格式。
     */
    private static String receivedAtToIso(JsonElement value) {
        if (value == null || value.isJsonNull()) {
            return "";
        }
        try {
            double seconds = value.getAsDouble();
            if (seconds <= 0) {
                return "";
            }
            return DateTimeFormatter.ISO_INSTANT.format(Instant.ofEpochSecond((long) seconds));
        } catch (RuntimeException e) {
            return "";
        }
    }

    private static Map<String, Object> flattenEmail(JsonObject raw, String recipient) {
        Map<String, Object> dict = new LinkedHashMap<>();
        dict.put("id", Json.str(raw, "id"));
        dict.put("from", Json.str(raw, "fromAddress"));
        String to = Json.str(raw, "toAddress");
        dict.put("to", to.isEmpty() ? recipient : to);
        dict.put("subject", Json.str(raw, "subject"));
        dict.put("text", Json.str(raw, "textContent"));
        dict.put("html", Json.str(raw, "htmlContent"));
        dict.put("date", receivedAtToIso(raw.has("receivedAt") ? raw.get("receivedAt") : null));
        dict.put("isRead", raw.has("isRead") && raw.get("isRead").isJsonPrimitive()
                && raw.get("isRead").getAsBoolean());
        return dict;
    }

    private static JsonObject fetchEmailDetail(String messageId) {
        if (messageId == null || messageId.isEmpty()) {
            return null;
        }
        try {
            HttpResult r = HttpClient.get(BASE + "/emails/" + ProviderUtil.urlEncode(messageId), GET_HEADERS);
            if (!r.isOk()) {
                return null;
            }
            JsonElement data = Json.parse(r.getBody());
            if (data == null || !data.isJsonObject()) {
                return null;
            }
            JsonObject root = data.getAsJsonObject();
            if (!root.has("success") || !root.get("success").getAsBoolean()) {
                return null;
            }
            JsonElement emailEl = root.get("email");
            return (emailEl != null && emailEl.isJsonObject()) ? emailEl.getAsJsonObject() : null;
        } catch (RuntimeException e) {
            return null;
        }
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("expiresInHours", 24);
        String payload = Json.serialize(body);
        HttpResult r = HttpClient.post(BASE + "/mailboxes", payload, "application/json", HEADERS);
        r.ensureSuccess();
        JsonElement data = Json.parse(r.getBody());
        if (data == null || !data.isJsonObject()) {
            throw new RuntimeException("mffac: 创建失败");
        }
        JsonObject root = data.getAsJsonObject();
        if (!root.has("success") || !root.get("success").getAsBoolean() || !root.has("mailbox")) {
            throw new RuntimeException("mffac: 创建失败");
        }
        JsonObject mb = root.getAsJsonObject("mailbox");
        String addr = Json.str(mb, "address").trim();
        String mid = Json.str(mb, "id").trim();
        if (addr.isEmpty() || mid.isEmpty()) {
            throw new RuntimeException("mffac: 无效的邮箱响应");
        }
        String email = addr + "@mffac.com";
        String createdAt = Json.str(mb, "createdAt");
        return new EmailInfo("mffac", email, mid, null, createdAt.isEmpty() ? null : createdAt);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token mailbox id（未使用，按地址查询）
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String local = email != null && email.contains("@") ? email.split("@", 2)[0].trim() : "";
        if (local.isEmpty()) {
            throw new RuntimeException("mffac: 邮箱地址为空");
        }
        HttpResult r = HttpClient.get(BASE + "/mailboxes/" + local + "/emails", GET_HEADERS);
        r.ensureSuccess();
        JsonElement data = Json.parse(r.getBody());
        if (data == null || !data.isJsonObject()) {
            throw new RuntimeException("mffac: 列表响应异常");
        }
        JsonObject root = data.getAsJsonObject();
        if (!root.has("success") || !root.get("success").getAsBoolean()) {
            throw new RuntimeException("mffac: 列表请求失败");
        }
        JsonArray rawList = Json.arr(root, "emails");
        List<Email> out = new ArrayList<>();
        if (rawList == null) {
            return out;
        }
        for (JsonElement item : rawList) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject raw = item.getAsJsonObject();
            String messageId = Json.str(raw, "id").trim();
            JsonObject detail = fetchEmailDetail(messageId);
            out.add(Normalizer.normalizeEmail(flattenEmail(detail != null ? detail : raw, email), email));
        }
        return out;
    }
}
