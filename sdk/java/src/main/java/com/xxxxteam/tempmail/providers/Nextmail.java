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
 * Nextmail 渠道（nextmail.sh）。
 *
 * <p>创建收件箱返回内部令牌；内部令牌格式为 {@code 收件箱ID|读取令牌}。
 * 详情正文位于 {@code message.decodedBody}（HTML）。</p>
 */
public final class Nextmail {

    private static final String API_BASE = "https://api.nextmail.sh";

    private Nextmail() {
    }

    private static Map<String, String> bearerHeaders(String bearer) {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("Accept", "application/json");
        headers.put("Authorization", "Bearer " + bearer);
        return headers;
    }

    /**
     * 租用 Nextmail 临时收件箱。
     *
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel) {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("Accept", "application/json");
        headers.put("Content-Type", "application/json");
        HttpResult resp = HttpClient.post(API_BASE + "/temp-mail/inboxes", "{}", "application/json", headers);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        String address = data == null ? "" : Json.str(data, "address").trim();
        String id = data == null ? "" : Json.str(data, "id").trim();
        String token = data == null ? "" : Json.str(data, "token").trim();
        if (address.isEmpty() || id.isEmpty() || token.isEmpty()) {
            throw new RuntimeException("nextmail: incomplete creation response");
        }
        return new EmailInfo(channel, address, id + "|" + token, null, Json.str(data, "createdAt"));
    }

    /**
     * 获取 Nextmail 邮件，详情正文位于 message.decodedBody。
     *
     * @param email 邮箱地址
     * @param token 内部令牌
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (token == null || !token.contains("|")) {
            throw new IllegalArgumentException("nextmail: malformed internal token");
        }
        String[] parts = token.split("\\|", 2);
        String base = API_BASE + "/temp-mail/inboxes/" + ProviderUtil.urlEncode(parts[0]) + "/messages";
        HttpResult resp = HttpClient.get(base + "?limit=50", bearerHeaders(parts[1]));
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        JsonArray rows = Json.arr(data, "data");
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
            JsonObject detail = fetchDetail(base, messageId, parts[1]);
            if (detail != null) {
                Map<String, Object> flat = new LinkedHashMap<>();
                flat.put("id", ProviderUtil.firstNonEmpty(Json.str(detail, "id"), messageId));
                flat.put("from", nestedEmail(detail, "from"));
                flat.put("to", ProviderUtil.firstNonEmpty(Json.str(detail, "to"), email));
                flat.put("subject", Json.str(detail, "subject"));
                flat.put("date", Json.str(detail, "receivedAt"));
                flat.put("seen", Json.str(detail, "seen"));
                if (detail.has("message") && detail.get("message").isJsonObject()) {
                    flat.put("html", Json.str(detail.getAsJsonObject("message"), "decodedBody"));
                }
                result.add(Normalizer.normalizeEmail(flat, email));
                continue;
            }
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", messageId);
            flat.put("from", nestedEmail(item, "from"));
            flat.put("to", ProviderUtil.firstNonEmpty(Json.str(item, "to"), email));
            flat.put("subject", Json.str(item, "subject"));
            flat.put("text", Json.str(item, "snippet"));
            flat.put("date", Json.str(item, "receivedAt"));
            flat.put("seen", Json.str(item, "seen"));
            result.add(Normalizer.normalizeEmail(flat, email));
        }
        return result;
    }

    private static String nestedEmail(JsonObject raw, String key) {
        if (!raw.has(key)) {
            return "";
        }
        JsonElement value = raw.get(key);
        if (value.isJsonObject()) {
            return Json.str(value.getAsJsonObject(), "email");
        }
        return value.isJsonPrimitive() ? value.getAsString() : "";
    }

    private static JsonObject fetchDetail(String base, String messageId, String bearer) {
        try {
            HttpResult resp = HttpClient.get(
                    base + "/" + ProviderUtil.urlEncode(messageId), bearerHeaders(bearer));
            if (!resp.isOk()) {
                return null;
            }
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException ignored) {
            return null;
        }
    }
}
