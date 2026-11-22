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
 * DummyInbox 渠道（dummyinbox.com）。
 *
 * <p>无需密钥；创建随机地址后，地址本身即读取凭据。</p>
 */
public final class DummyInbox {

    private static final String API_BASE = "https://dummyinbox.com/api/v1";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
    }

    private DummyInbox() {
    }

    /**
     * 创建 DummyInbox 临时邮箱。
     *
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel) {
        HttpResult resp = HttpClient.post(API_BASE + "/inboxes", null, null, HEADERS);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        String address = data == null ? "" : Json.str(data, "address").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("dummyinbox: empty address");
        }
        return new EmailInfo(channel, address);
    }

    /**
     * 获取 DummyInbox 邮件，逐条补全正文。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("dummyinbox: empty email");
        }
        String address = email.trim();
        String pathAddress = ProviderUtil.urlEncode(address).replace("%40", "@");
        HttpResult resp = HttpClient.get(
                API_BASE + "/inboxes/" + pathAddress + "/messages?limit=50", HEADERS);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        JsonArray rows = Json.arr(data, "messages");
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
            JsonObject detail = fetchDetail(pathAddress, messageId);
            if (detail != null) {
                result.add(Normalizer.normalizeEmail(
                        flattenDetail(detail, address), address));
                continue;
            }
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", messageId);
            flat.put("from", Json.str(item, "from"));
            flat.put("to", address);
            flat.put("subject", Json.str(item, "subject"));
            flat.put("text", Json.str(item, "snippet"));
            flat.put("date", Json.str(item, "receivedAt"));
            result.add(Normalizer.normalizeEmail(flat, address));
        }
        return result;
    }

    private static JsonObject fetchDetail(String pathAddress, String messageId) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/inboxes/" + pathAddress + "/messages/"
                            + ProviderUtil.urlEncode(messageId), HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            JsonObject raw = Json.parseObject(resp.getBody());
            if (raw == null || !raw.has("message") || !raw.get("message").isJsonObject()) {
                return null;
            }
            return raw.getAsJsonObject("message");
        } catch (RuntimeException ignored) {
            return null;
        }
    }

    private static Map<String, Object> flattenDetail(JsonObject raw, String recipient) {
        Map<String, Object> flat = new LinkedHashMap<>();
        flat.put("id", Json.str(raw, "id"));
        flat.put("from", Json.str(raw, "from"));
        String to = "";
        if (raw.has("to") && raw.get("to").isJsonArray() && raw.getAsJsonArray("to").size() > 0) {
            JsonElement first = raw.getAsJsonArray("to").get(0);
            to = first.isJsonPrimitive() ? first.getAsString() : "";
        }
        flat.put("to", to.isEmpty() ? recipient : to);
        flat.put("subject", Json.str(raw, "subject"));
        flat.put("text", Json.str(raw, "text"));
        flat.put("html", Json.str(raw, "html"));
        flat.put("date", Json.str(raw, "receivedAt"));
        JsonArray attachments = Json.arr(raw, "attachments");
        if (attachments != null) {
            flat.put("attachments", attachments);
        }
        return flat;
    }
}
