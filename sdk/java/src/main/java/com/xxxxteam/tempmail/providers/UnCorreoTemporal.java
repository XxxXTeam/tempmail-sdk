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
 * UnCorreoTemporal 渠道 — https://uncorreotemporal.com
 * POST /api/v1/mailboxes 创建，GET /api/v1/mailboxes/{email}/messages 收信。
 */
public final class UnCorreoTemporal {

    private static final String API_BASE = "https://uncorreotemporal.com/api/v1";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("Origin", "https://uncorreotemporal.com");
        HEADERS.put("Referer", "https://uncorreotemporal.com/");
        HEADERS.put("User-Agent", "Mozilla/5.0");
    }

    private UnCorreoTemporal() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> h = new LinkedHashMap<>(HEADERS);
        h.put("Content-Type", "application/json");
        HttpResult resp = HttpClient.post(API_BASE + "/mailboxes", null, null, h);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String email = Json.str(data, "address").trim();
        String token = Json.str(data, "session_token").trim();
        if (email.isEmpty() || !email.contains("@") || token.isEmpty()) {
            throw new RuntimeException("uncorreotemporal: 创建邮箱响应无效");
        }
        return new EmailInfo("uncorreotemporal", email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token 会话令牌
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String sessionToken = (token != null ? token : "").trim();
        String address = (email != null ? email : "").trim();
        if (sessionToken.isEmpty()) {
            throw new RuntimeException("uncorreotemporal: session token 为空");
        }
        if (address.isEmpty()) {
            throw new RuntimeException("uncorreotemporal: 邮箱地址为空");
        }
        Map<String, String> h = new LinkedHashMap<>(HEADERS);
        h.put("X-Session-Token", sessionToken);
        HttpResult resp = HttpClient.get(
                API_BASE + "/mailboxes/" + ProviderUtil.urlEncode(address) + "/messages?limit=50", h);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonArray()) {
            return result;
        }
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (item == null || !item.isJsonObject()) {
                continue;
            }
            String msgId = Json.str(item, "id").trim();
            JsonElement detail = null;
            if (!msgId.isEmpty()) {
                detail = fetchDetail(address, sessionToken, msgId);
            }
            Map<String, Object> raw = buildRaw(detail != null ? detail : item, address);
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static JsonElement fetchDetail(String email, String token, String messageId) {
        try {
            Map<String, String> h = new LinkedHashMap<>(HEADERS);
            h.put("X-Session-Token", token);
            HttpResult resp = HttpClient.get(
                    API_BASE + "/mailboxes/" + ProviderUtil.urlEncode(email)
                            + "/messages/" + ProviderUtil.urlEncode(messageId), h);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parse(resp.getBody());
        } catch (Exception e) {
            return null;
        }
    }

    private static Map<String, Object> buildRaw(JsonElement el, String recipient) {
        Map<String, Object> raw = Json.toDict(el);
        raw.putIfAbsent("from", raw.getOrDefault("from_address", ""));
        raw.putIfAbsent("to", raw.getOrDefault("to_address", recipient));
        raw.putIfAbsent("text", raw.getOrDefault("body_text", ""));
        raw.putIfAbsent("html", raw.getOrDefault("body_html", ""));
        raw.putIfAbsent("date", raw.getOrDefault("received_at", ""));
        return raw;
    }
}
