package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
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
 * Tempmailify 渠道（tempmailify.com）。
 *
 * <p>创建邮箱返回收件箱专属 Bearer 令牌；详情提供完整 text/html。</p>
 */
public final class Tempmailify {

    private static final String API_BASE = "https://tempmailify.com";

    private Tempmailify() {
    }

    private static Map<String, String> bearerHeaders(String bearer) {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("Accept", "application/json");
        headers.put("Authorization", "Bearer " + bearer);
        return headers;
    }

    private static String fromAddress(JsonObject raw) {
        if (!raw.has("from")) {
            return "";
        }
        JsonElement value = raw.get("from");
        if (value.isJsonObject()) {
            return Json.str(value.getAsJsonObject(), "address");
        }
        return value.isJsonPrimitive() ? value.getAsString() : "";
    }

    /**
     * 创建 Tempmailify 临时邮箱。
     *
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel) {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("Accept", "application/json");
        headers.put("Content-Type", "application/json");
        HttpResult resp = HttpClient.post(API_BASE + "/api/inbox", "{}", "application/json", headers);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        String address = data == null ? "" : Json.str(data, "address").trim();
        String token = data == null ? "" : Json.str(data, "token").trim();
        if (address.isEmpty() || token.isEmpty()) {
            throw new RuntimeException("tempmailify: incomplete creation response");
        }
        return new EmailInfo(channel, address, token, null,
                data.get("createdAt") == null ? null : data.get("createdAt").toString());
    }

    /**
     * 获取 Tempmailify 邮件，详情提供完整 text/html。
     *
     * @param email 邮箱地址
     * @param token 收件箱令牌
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (token == null || token.isBlank()) {
            throw new IllegalArgumentException("tempmailify: empty token");
        }
        String base = API_BASE + "/api/inbox/" + ProviderUtil.urlEncode(email.trim());
        HttpResult resp = HttpClient.get(base, bearerHeaders(token));
        resp.ensureSuccess();
        JsonArray rows = Json.arr(Json.parseObject(resp.getBody()), "messages");
        List<Email> result = new ArrayList<>();
        if (rows == null) {
            return result;
        }
        for (JsonElement row : rows) {
            if (!row.isJsonObject()) {
                continue;
            }
            JsonObject item = row.getAsJsonObject();
            String messageId = Json.str(item, "id").trim();
            if (messageId.isEmpty()) {
                continue;
            }
            JsonObject detail = fetchDetail(base, messageId, token);
            if (detail != null) {
                Map<String, Object> flat = new LinkedHashMap<>();
                flat.put("id", ProviderUtil.firstNonEmpty(Json.str(detail, "id"), messageId));
                flat.put("from", fromAddress(detail));
                flat.put("to", ProviderUtil.firstNonEmpty(Json.str(detail, "to"), email));
                flat.put("subject", ProviderUtil.firstNonEmpty(
                        Json.str(detail, "subject"), Json.str(item, "subject")));
                flat.put("text", Json.str(detail, "text"));
                flat.put("html", Json.str(detail, "html"));
                flat.put("date", ProviderUtil.firstNonEmpty(
                        Json.str(detail, "receivedAt"), Json.str(item, "receivedAt")));
                flat.put("seen", Json.str(detail, "seen"));
                JsonArray attachments = Json.arr(detail, "attachments");
                if (attachments != null) {
                    flat.put("attachments", attachments);
                }
                result.add(Normalizer.normalizeEmail(flat, email));
                continue;
            }
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", messageId);
            flat.put("from", fromAddress(item));
            flat.put("to", email);
            flat.put("subject", Json.str(item, "subject"));
            flat.put("text", Json.str(item, "preview"));
            flat.put("date", Json.str(item, "receivedAt"));
            flat.put("seen", Json.str(item, "seen"));
            result.add(Normalizer.normalizeEmail(flat, email));
        }
        return result;
    }

    private static JsonObject fetchDetail(String base, String messageId, String token) {
        try {
            HttpResult resp = HttpClient.get(
                    base + "/messages/" + ProviderUtil.urlEncode(messageId), bearerHeaders(token));
            if (!resp.isOk()) {
                return null;
            }
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException ignored) {
            return null;
        }
    }
}
