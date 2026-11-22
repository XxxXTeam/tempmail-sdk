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
 * temp.tf 渠道（temp.tf）。
 *
 * <p>创建返回一次性地址；收件列表响应按 bodyContentType 归位正文。</p>
 */
public final class TempTF {

    private static final String API_BASE = "https://temp.tf/api";

    private TempTF() {
    }

    private static Map<String, String> headers() {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("Accept", "application/json");
        headers.put("Content-Type", "application/json");
        return headers;
    }

    /**
     * 获取一个一次性地址。
     *
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel) {
        HttpResult resp = HttpClient.get(API_BASE + "/account?plus=1", headers());
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        String address = data == null ? "" : Json.str(data, "email").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("temp-tf: empty email");
        }
        return new EmailInfo(channel, address);
    }

    /**
     * 获取 temp.tf 邮件，body 按 bodyContentType 归位。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("temp-tf: empty email");
        }
        Map<String, Object> payload = new LinkedHashMap<>();
        payload.put("email", email);
        HttpResult resp = HttpClient.post(API_BASE + "/check", Json.serialize(payload),
                "application/json", headers());
        resp.ensureSuccess();
        JsonArray rows = Json.arr(Json.parseObject(resp.getBody()), "data");
        List<Email> result = new ArrayList<>();
        if (rows == null) {
            return result;
        }
        for (JsonElement row : rows) {
            if (!row.isJsonObject()) {
                continue;
            }
            JsonObject item = row.getAsJsonObject();
            boolean isText = "text".equalsIgnoreCase(Json.str(item, "bodyContentType"));
            String body = Json.str(item, "body");
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", Json.str(item, "id"));
            flat.put("from", Json.str(item, "from"));
            flat.put("to", email);
            flat.put("subject", Json.str(item, "subject"));
            flat.put("text", isText ? body : "");
            flat.put("html", isText ? "" : body);
            flat.put("date", Json.str(item, "date"));
            JsonArray attachments = Json.arr(item, "attachments");
            if (attachments != null) {
                flat.put("attachments", attachments);
            }
            result.add(Normalizer.normalizeEmail(flat, email));
        }
        return result;
    }
}
