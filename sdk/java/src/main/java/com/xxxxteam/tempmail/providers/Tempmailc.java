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
 * TempMailC 渠道实现 — https://tempmailc.com
 */
public final class Tempmailc {

    private static final String API_BASE = "https://tempmailc.com/api/v1";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
    }

    private Tempmailc() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(API_BASE + "/new", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonObject()) {
            throw new RuntimeException("tempmailc: 无效的创建响应");
        }
        JsonObject root = data.getAsJsonObject();
        if (!root.has("ok") || !root.get("ok").getAsBoolean()) {
            throw new RuntimeException("tempmailc: 创建邮箱失败");
        }
        String email = Json.str(root, "email").trim();
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("tempmailc: 无效的邮箱地址");
        }
        return new EmailInfo("tempmailc", email);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("tempmailc: 邮箱地址为空");
        }
        HttpResult resp = HttpClient.get(
                API_BASE + "/inbox?email=" + ProviderUtil.urlEncode(address), HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonObject()) {
            throw new RuntimeException("tempmailc: 收件箱响应异常");
        }
        JsonObject root = data.getAsJsonObject();
        if (!root.has("ok") || !root.get("ok").getAsBoolean()) {
            throw new RuntimeException("tempmailc: 收件箱请求失败");
        }
        JsonArray rows = Json.arr(root, "messages");
        List<Email> result = new ArrayList<>();
        if (rows == null) {
            return result;
        }
        for (JsonElement item : rows) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject row = item.getAsJsonObject();
            String messageId = Json.str(row, "id").trim();
            JsonObject detail = fetchMessage(address, messageId);
            result.add(Normalizer.normalizeEmail(
                    detail != null ? Json.toDict(detail) : flattenListMessage(row, address), address));
        }
        return result;
    }

    private static JsonObject fetchMessage(String email, String messageId) {
        if (messageId.isEmpty()) {
            return null;
        }
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/message?email=" + ProviderUtil.urlEncode(email)
                            + "&msg_id=" + ProviderUtil.urlEncode(messageId), HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            JsonElement data = Json.parse(resp.getBody());
            if (data == null || !data.isJsonObject()) {
                return null;
            }
            JsonObject root = data.getAsJsonObject();
            if (root.has("ok") && root.get("ok").isJsonPrimitive()
                    && !root.get("ok").getAsBoolean()) {
                return null;
            }
            return root;
        } catch (RuntimeException e) {
            return null;
        }
    }

    private static Map<String, Object> flattenListMessage(JsonObject raw, String recipient) {
        Map<String, Object> dict = new LinkedHashMap<>();
        dict.put("id", Json.str(raw, "id"));
        dict.put("from", Json.str(raw, "from"));
        dict.put("to", recipient);
        dict.put("subject", Json.str(raw, "subject"));
        dict.put("timestamp", Json.str(raw, "ts"));
        dict.put("read", Json.str(raw, "read"));
        return dict;
    }
}
