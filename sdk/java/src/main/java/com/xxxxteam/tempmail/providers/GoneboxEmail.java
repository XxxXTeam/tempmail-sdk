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
 * Gonebox Email 渠道（gonebox.email）。
 * 一次性临时邮箱服务，无需认证。
 */
public final class GoneboxEmail {

    private static final String BASE_URL = "https://api.gonebox.email/api/v1";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();
    private static final Map<String, String> GET_HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
        HEADERS.put("Accept", "application/json");
        HEADERS.put("Content-Type", "application/json");
        GET_HEADERS.put("User-Agent", HEADERS.get("User-Agent"));
        GET_HEADERS.put("Accept", "application/json");
    }

    private GoneboxEmail() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String body = Json.serialize(Map.of("domain", "gonebox.email"));
        HttpResult resp = HttpClient.post(BASE_URL + "/inboxes", body, "application/json", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("gonebox-email: 创建邮箱失败");
        }
        JsonObject root = parsed.getAsJsonObject();
        if (!root.has("success") || !root.get("success").getAsBoolean()) {
            throw new RuntimeException("gonebox-email: 创建邮箱失败");
        }
        JsonElement dataEl = root.get("data");
        if (dataEl == null || !dataEl.isJsonObject()) {
            throw new RuntimeException("gonebox-email: 响应 data 格式无效");
        }
        String address = Json.str(dataEl, "address").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("gonebox-email: 缺少邮箱地址");
        }
        Long expiresAt = null;
        String rawExpires = Json.str(dataEl, "expiresAt").trim();
        if (!rawExpires.isEmpty()) {
            try {
                expiresAt = Long.parseLong(rawExpires) * 1000;
            } catch (NumberFormatException ignored) {
            }
        }
        return new EmailInfo("gonebox-email", address, "", expiresAt, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token token（空串）
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("gonebox-email: 缺少邮箱地址");
        }
        HttpResult resp = HttpClient.get(
                BASE_URL + "/inboxes/" + ProviderUtil.urlEncode(address) + "/messages", GET_HEADERS);
        if (resp.getStatusCode() == 404) {
            return new ArrayList<>();
        }
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonObject()) {
            return result;
        }
        JsonObject root = parsed.getAsJsonObject();
        if (!root.has("success") || !root.get("success").getAsBoolean()) {
            return result;
        }
        JsonElement dataEl = root.get("data");
        if (dataEl == null || !dataEl.isJsonObject()) {
            return result;
        }
        JsonElement msgsEl = dataEl.getAsJsonObject().get("messages");
        if (msgsEl == null || !msgsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement item : msgsEl.getAsJsonArray()) {
            if (!item.isJsonObject()) {
                continue;
            }
            Map<String, Object> msg = Json.toDict(item);
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", msg.getOrDefault("id", ""));
            raw.put("from", ProviderUtil.firstNonEmpty(
                    str(msg, "from"), str(msg, "sender")));
            raw.put("to", address);
            raw.put("subject", msg.getOrDefault("subject", ""));
            raw.put("text", ProviderUtil.firstNonEmpty(str(msg, "text"), str(msg, "body_plain")));
            raw.put("html", ProviderUtil.firstNonEmpty(str(msg, "html"), str(msg, "body_html")));
            raw.put("date", ProviderUtil.firstNonEmpty(str(msg, "date"), str(msg, "received_at")));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static String str(Map<String, Object> m, String key) {
        Object v = m.get(key);
        return v != null ? v.toString() : "";
    }
}
