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
 * MailboxTemp 渠道（mailboxtemp.com）。
 *
 * <p>无需密钥；创建免费邮箱后，地址即读取凭据。</p>
 */
public final class MailboxTemp {

    private static final String API_BASE = "https://mailboxtemp.com";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
    }

    private MailboxTemp() {
    }

    /**
     * 创建 MailboxTemp 临时邮箱。
     *
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel) {
        HttpResult resp = HttpClient.post(API_BASE + "/api/inbox/generate", null, null, HEADERS);
        JsonObject data = Json.parseObject(resp.getBody());
        String address = data == null ? "" : Json.str(data, "address").trim();
        if (!resp.isOk() || address.isEmpty()) {
            throw new RuntimeException("mailboxtemp: create inbox failed HTTP " + resp.getStatusCode());
        }
        return new EmailInfo(channel, address);
    }

    /**
     * 获取 MailboxTemp 邮件，逐条补全正文。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("mailboxtemp: empty email");
        }
        String address = email.trim();
        String pathAddress = ProviderUtil.urlEncode(address);
        JsonObject data = fetchJson(API_BASE + "/api/inbox/" + pathAddress + "/emails");
        JsonArray rows = Json.arr(data, "emails");
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
            JsonObject detail = fetchJson(API_BASE + "/api/email/" + pathAddress + "/"
                    + ProviderUtil.urlEncode(messageId));
            if (detail != null && detail.has("email") && detail.get("email").isJsonObject()) {
                JsonObject message = detail.getAsJsonObject("email");
                Map<String, Object> flat = new LinkedHashMap<>();
                flat.put("id", Json.str(message, "id"));
                flat.put("from", Json.str(message, "from_address"));
                flat.put("to", address);
                flat.put("subject", Json.str(message, "subject"));
                flat.put("text", Json.str(message, "body_text"));
                flat.put("html", Json.str(message, "body_html"));
                flat.put("date", Json.str(message, "received_at"));
                flat.put("read", Json.str(message, "read"));
                JsonArray attachments = Json.arr(detail, "attachments");
                if (attachments != null) {
                    flat.put("attachments", attachments);
                }
                result.add(Normalizer.normalizeEmail(flat, address));
                continue;
            }
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", messageId);
            flat.put("from", Json.str(item, "from_address"));
            flat.put("to", address);
            flat.put("subject", Json.str(item, "subject"));
            flat.put("date", Json.str(item, "received_at"));
            flat.put("read", Json.str(item, "read"));
            result.add(Normalizer.normalizeEmail(flat, address));
        }
        return result;
    }

    private static JsonObject fetchJson(String url) {
        try {
            HttpResult resp = HttpClient.get(url, HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            JsonObject data = Json.parseObject(resp.getBody());
            if (data == null || (data.has("ok") && !data.get("ok").getAsBoolean())) {
                return null;
            }
            return data;
        } catch (RuntimeException ignored) {
            return null;
        }
    }
}
