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
 * Tsukaisute Mail 渠道（tsukaisutemail.com）。
 *
 * <p>无需密钥；创建随机地址后，地址即读取凭据。</p>
 */
public final class TsukaisuteMail {

    private static final String API_BASE = "https://tsukaisutemail.com/api/v1";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
    }

    private TsukaisuteMail() {
    }

    /**
     * 创建 Tsukaisute Mail 临时地址。
     *
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel) {
        HttpResult resp = HttpClient.post(API_BASE + "/address", null, null, HEADERS);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        String address = data == null ? "" : Json.str(data, "address").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("tsukaisutemail: empty address");
        }
        return new EmailInfo(channel, address);
    }

    /**
     * 获取 Tsukaisute Mail 邮件，逐条补全正文。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("tsukaisutemail: empty email");
        }
        String address = email.trim();
        String pathAddress = ProviderUtil.urlEncode(address).replace("%40", "@");
        HttpResult resp = HttpClient.get(API_BASE + "/inbox/" + pathAddress, HEADERS);
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
            JsonObject detail = fetchDetail(messageId);
            if (detail != null && detail.has("message") && detail.get("message").isJsonObject()) {
                JsonObject message = detail.getAsJsonObject("message");
                Map<String, Object> flat = new LinkedHashMap<>();
                flat.put("id", Json.str(message, "id"));
                flat.put("from", Json.str(message, "from_address"));
                flat.put("to", address);
                flat.put("subject", Json.str(message, "subject"));
                flat.put("text", Json.str(message, "text_body"));
                flat.put("html", Json.str(message, "html_body"));
                flat.put("date", Json.str(message, "received_at"));
                result.add(Normalizer.normalizeEmail(flat, address));
                continue;
            }
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", messageId);
            flat.put("from", Json.str(item, "from_address"));
            flat.put("to", address);
            flat.put("subject", Json.str(item, "subject"));
            flat.put("text", Json.str(item, "snippet"));
            flat.put("date", Json.str(item, "received_at"));
            result.add(Normalizer.normalizeEmail(flat, address));
        }
        return result;
    }

    private static JsonObject fetchDetail(String messageId) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/message/" + ProviderUtil.urlEncode(messageId), HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException ignored) {
            return null;
        }
    }
}
