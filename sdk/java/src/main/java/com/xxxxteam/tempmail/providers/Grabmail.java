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
 * GrabMail 渠道（grabmail.io）。
 *
 * <p>地址即邮箱：首封邮件到达时自动存在，无需创建。</p>
 */
public final class Grabmail {

    private static final String API_BASE = "https://grabmail.io/api/v1";
    private static final String DOMAIN = "grabmail.io";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
    }

    private Grabmail() {
    }

    private static String fromValue(JsonObject raw) {
        String name = Json.str(raw, "from_name");
        String address = Json.str(raw, "from");
        return name.isEmpty() ? address : name + " <" + address + ">";
    }

    /**
     * 生成 GrabMail 地址（无需请求服务端）。
     *
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel) {
        String local = "sdk" + ProviderUtil.randomString(16);
        return new EmailInfo(channel, local + "@" + DOMAIN);
    }

    /**
     * 获取 GrabMail 邮件，逐条补全正文。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("grabmail: empty email");
        }
        String address = email.trim();
        HttpResult resp = HttpClient.get(
                API_BASE + "/mailbox?address=" + ProviderUtil.urlEncode(address), HEADERS);
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
            JsonObject detail = fetchDetail(messageId, address);
            if (detail != null) {
                Map<String, Object> flat = new LinkedHashMap<>();
                flat.put("id", ProviderUtil.firstNonEmpty(Json.str(detail, "id"), messageId));
                flat.put("from", fromValue(detail));
                flat.put("to", address);
                flat.put("subject", Json.str(detail, "subject"));
                flat.put("text", Json.str(detail, "text"));
                flat.put("html", Json.str(detail, "html"));
                flat.put("date", Json.str(detail, "date"));
                flat.put("seen", Json.str(detail, "seen"));
                JsonArray attachments = Json.arr(detail, "attachments");
                if (attachments != null) {
                    flat.put("attachments", attachments);
                }
                result.add(Normalizer.normalizeEmail(flat, address));
                continue;
            }
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", messageId);
            flat.put("from", fromValue(item));
            flat.put("to", address);
            flat.put("subject", Json.str(item, "subject"));
            flat.put("text", Json.str(item, "preview"));
            flat.put("date", Json.str(item, "date"));
            flat.put("seen", Json.str(item, "seen"));
            result.add(Normalizer.normalizeEmail(flat, address));
        }
        return result;
    }

    private static JsonObject fetchDetail(String messageId, String address) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/message/" + ProviderUtil.urlEncode(messageId)
                            + "?mailbox=" + ProviderUtil.urlEncode(address),
                    HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException ignored) {
            return null;
        }
    }
}
