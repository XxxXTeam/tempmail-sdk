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
 * TempMailPro 渠道 — https://tempmailpro.us
 * POST /api/v1/mailbox/create 创建邮箱，GET /api/v1/mailbox/{token}/emails 获取邮件。
 */
public final class TempMailPro {

    private static final String API_BASE = "https://tempmailpro.us/api/v1";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0");
    }

    private TempMailPro() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> h = new LinkedHashMap<>(HEADERS);
        h.put("Content-Type", "application/json");
        HttpResult resp = HttpClient.post(API_BASE + "/mailbox/create", "{}", "application/json", h);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonElement box = data != null && data.isJsonObject() ? data.getAsJsonObject().get("data") : null;
        String email = box != null ? Json.str(box, "address").trim() : "";
        String token = box != null ? Json.str(box, "token").trim() : "";
        if (email.isEmpty() || !email.contains("@") || token.isEmpty()) {
            throw new RuntimeException("tempmailpro: 创建邮箱响应无效");
        }
        return new EmailInfo("tempmailpro", email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token 邮箱令牌
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String mailboxToken = (token != null ? token : "").trim();
        String address = (email != null ? email : "").trim();
        if (mailboxToken.isEmpty()) {
            throw new RuntimeException("tempmailpro: token 为空");
        }
        if (address.isEmpty()) {
            throw new RuntimeException("tempmailpro: 邮箱地址为空");
        }
        HttpResult resp = HttpClient.get(
                API_BASE + "/mailbox/" + ProviderUtil.urlEncode(mailboxToken) + "/emails", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonArray rows = Json.arr(data, "data");
        if (rows == null) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : rows) {
            if (item == null || !item.isJsonObject()) {
                continue;
            }
            String msgId = Json.str(item, "id").trim();
            JsonElement detail = null;
            if (!msgId.isEmpty()) {
                detail = fetchDetail(mailboxToken, msgId);
            }
            Map<String, Object> raw = buildRaw(detail != null ? detail : item, address);
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static JsonElement fetchDetail(String token, String messageId) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/mailbox/" + ProviderUtil.urlEncode(token)
                            + "/emails/" + ProviderUtil.urlEncode(messageId), HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            JsonElement data = Json.parse(resp.getBody());
            if (data != null && data.isJsonObject() && data.getAsJsonObject().has("data")) {
                return data.getAsJsonObject().get("data");
            }
            return null;
        } catch (Exception e) {
            return null;
        }
    }

    private static Map<String, Object> buildRaw(JsonElement el, String recipient) {
        Map<String, Object> raw = Json.toDict(el);
        raw.putIfAbsent("from", raw.getOrDefault("from_address", raw.getOrDefault("from_name", "")));
        raw.putIfAbsent("to", recipient);
        raw.putIfAbsent("text", raw.getOrDefault("body_text", ""));
        raw.putIfAbsent("html", raw.getOrDefault("body_html", ""));
        raw.putIfAbsent("date", raw.getOrDefault("received_at", ""));
        return raw;
    }
}
