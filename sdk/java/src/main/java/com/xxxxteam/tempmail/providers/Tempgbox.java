package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;

/**
 * TempGBox 渠道 — https://tempgbox.net/api/proxy
 * JSON API，响应以 base64 编码嵌入 HTML data-x 属性中。
 */
public final class Tempgbox {

    private static final String CHANNEL = "tempgbox";
    private static final String API_URL = "https://tempgbox.net/api/proxy";
    private static final String ORIGIN = "https://tempgbox.net";

    private Tempgbox() {
    }

    private static Map<String, String> defaultHeaders(String deviceId) {
        String ip = randomIp();
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html,application/json");
        h.put("Content-Type", "application/json");
        h.put("Origin", ORIGIN);
        h.put("Referer", ORIGIN + "/");
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36");
        h.put("X-Device-ID", deviceId);
        h.put("X-Forwarded-For", ip);
        h.put("X-Real-IP", ip);
        h.put("X-Originating-IP", ip);
        return h;
    }

    private static String randomDeviceId() {
        byte[] bytes = new byte[32];
        ThreadLocalRandom.current().nextBytes(bytes);
        StringBuilder sb = new StringBuilder(64);
        for (byte b : bytes) {
            sb.append(String.format("%02x", b & 0xFF));
        }
        return sb.toString();
    }

    private static String randomIp() {
        ThreadLocalRandom r = ThreadLocalRandom.current();
        return (r.nextInt(1, 255)) + "." + (r.nextInt(1, 255)) + "."
                + (r.nextInt(1, 255)) + "." + (r.nextInt(1, 255));
    }

    private static JsonObject decodePayload(String html) {
        // 查找 data-x="..." 或 data-x='...'
        String marker = "data-x=\"";
        int start = html.indexOf(marker);
        String quote = "\"";
        if (start < 0) {
            marker = "data-x='";
            start = html.indexOf(marker);
            quote = "'";
        }
        if (start < 0) {
            throw new RuntimeException("tempgbox: missing encoded response payload");
        }
        start += marker.length();
        int end = html.indexOf(quote, start);
        if (end < 0) {
            throw new RuntimeException("tempgbox: malformed encoded response payload");
        }
        String encoded = html.substring(start, end);
        String decoded = new String(Base64.getDecoder().decode(encoded), StandardCharsets.UTF_8);
        JsonElement parsed = Json.parse(decoded);
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("tempgbox: payload 解析失败");
        }
        return parsed.getAsJsonObject();
    }

    private static JsonObject postProxy(String route, String deviceId, Map<String, Object> payload) {
        String url = API_URL + "?route=" + ProviderUtil.urlEncode(route);
        HttpResult resp = HttpClient.post(url, Json.serialize(payload), "application/json",
                defaultHeaders(deviceId));
        JsonObject data = decodePayload(resp.getBody());
        if (!resp.isOk()) {
            String reason = ProviderUtil.firstNonEmpty(
                    Json.str(data, "detail"), Json.str(data, "error"), Json.str(data, "message"));
            throw new RuntimeException("tempgbox " + route + " failed: " + resp.getStatusCode() + " " + reason);
        }
        return data;
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String deviceId = randomDeviceId();
        Map<String, Object> payload = new LinkedHashMap<>();
        payload.put("variant", "googlemail");
        JsonObject data = postProxy("generate", deviceId, payload);

        JsonElement aliasEl = data.get("alias");
        if (aliasEl == null || !aliasEl.isJsonObject()) {
            throw new RuntimeException("tempgbox: missing alias in response");
        }
        JsonObject alias = aliasEl.getAsJsonObject();
        String email = ProviderUtil.firstNonEmpty(Json.str(alias, "email"), Json.str(alias, "alias"));
        if (email.isEmpty()) {
            throw new RuntimeException("tempgbox: missing email");
        }

        String createdAt = Json.str(alias, "created_at");
        String expiresAt = Json.str(alias, "expires_at");
        return new EmailInfo(CHANNEL, email, deviceId,
                expiresAt.isEmpty() ? null : null, createdAt.isEmpty() ? null : createdAt);
    }

    /**
     * 获取邮件列表。
     *
     * @param token    device ID
     * @param email    邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) {
            throw new IllegalArgumentException("tempgbox: missing device id");
        }
        Map<String, Object> payload = new LinkedHashMap<>();
        payload.put("email", email != null ? email : "");
        JsonObject data = postProxy("inbox", token, payload);

        JsonElement msgsEl = data.get("messages");
        if (msgsEl == null || !msgsEl.isJsonArray()) return new ArrayList<>();

        List<Email> result = new ArrayList<>();
        for (JsonElement msgEl : msgsEl.getAsJsonArray()) {
            if (!msgEl.isJsonObject()) continue;
            JsonObject raw = msgEl.getAsJsonObject();
            Map<String, Object> flat = Json.toDict(msgEl);
            // 映射字段
            flat.put("from", ProviderUtil.firstNonEmpty(Json.str(raw, "from"), Json.str(raw, "sender")));
            flat.put("text", ProviderUtil.firstNonEmpty(Json.str(raw, "text"), Json.str(raw, "body_text")));
            flat.put("html", ProviderUtil.firstNonEmpty(Json.str(raw, "html"), Json.str(raw, "body_html")));
            flat.put("date", ProviderUtil.firstNonEmpty(Json.str(raw, "date"), Json.str(raw, "received_at")));
            flat.put("messageId", ProviderUtil.firstNonEmpty(
                    Json.str(raw, "messageId"), Json.str(raw, "message_id"), Json.str(raw, "id")));
            result.add(Normalizer.normalizeEmail(flat, email));
        }
        return result;
    }
}
