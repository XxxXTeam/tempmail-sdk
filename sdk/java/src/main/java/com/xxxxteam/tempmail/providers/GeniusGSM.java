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
 * GeniusGSM 渠道（tempmail.geniusgsm.com）。
 *
 * <p>创建收件箱返回内部 ID；收件箱列表响应已包含 body_text / body_html。</p>
 */
public final class GeniusGSM {

    private static final String API_BASE = "https://tempmail.geniusgsm.com/api";

    private GeniusGSM() {
    }

    private static Map<String, String> headers() {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("Accept", "application/json");
        headers.put("Content-Type", "application/json");
        return headers;
    }

    private static JsonArray rows(JsonElement node) {
        if (node == null) {
            return null;
        }
        if (node.isJsonArray()) {
            return node.getAsJsonArray();
        }
        if (node.isJsonObject()) {
            JsonObject obj = node.getAsJsonObject();
            if (obj.has("results") && obj.get("results").isJsonArray()) {
                return obj.getAsJsonArray("results");
            }
        }
        return null;
    }

    private static String fromValue(JsonObject raw) {
        String name = Json.str(raw, "from_name");
        String address = Json.str(raw, "from_email");
        return name.isEmpty() || address.isEmpty() ? address : name + " <" + address + ">";
    }

    /**
     * 选择激活域名并创建收件箱。
     *
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel) {
        HttpResult domainResp = HttpClient.get(API_BASE + "/domains/", headers());
        domainResp.ensureSuccess();
        JsonArray domains = rows(Json.parse(domainResp.getBody()));
        int domainId = 0;
        if (domains != null) {
            for (JsonElement item : domains) {
                if (!item.isJsonObject()) {
                    continue;
                }
                JsonObject obj = item.getAsJsonObject();
                if (obj.has("is_active") && obj.get("is_active").getAsBoolean()) {
                    domainId = obj.get("id").getAsInt();
                    break;
                }
            }
        }
        if (domainId == 0) {
            throw new RuntimeException("geniusgsm: no active domain");
        }

        Map<String, Object> body = new LinkedHashMap<>();
        body.put("domain_id", domainId);
        body.put("ttl", 3600);
        HttpResult resp = HttpClient.post(
                API_BASE + "/inboxes/create_inbox/", Json.serialize(body), "application/json", headers());
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        String id = data == null ? "" : Json.str(data, "id").trim();
        String email = data == null ? "" : Json.str(data, "email").trim();
        if (id.isEmpty() || email.isEmpty()) {
            throw new RuntimeException("geniusgsm: incomplete creation response");
        }
        return new EmailInfo(channel, email, id, null,
                data.get("created_at") == null ? null : data.get("created_at").toString());
    }

    /**
     * 获取 GeniusGSM 邮件，列表响应已包含完整正文。
     *
     * @param email 邮箱地址
     * @param token 收件箱 ID
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (token == null || token.isBlank()) {
            throw new IllegalArgumentException("geniusgsm: empty inbox id");
        }
        HttpResult resp = HttpClient.get(
                API_BASE + "/inboxes/" + ProviderUtil.urlEncode(token) + "/emails/", headers());
        resp.ensureSuccess();
        JsonArray rows = rows(Json.parse(resp.getBody()));
        List<Email> result = new ArrayList<>();
        if (rows == null) {
            return result;
        }
        for (JsonElement row : rows) {
            if (!row.isJsonObject()) {
                continue;
            }
            JsonObject item = row.getAsJsonObject();
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", Json.str(item, "id"));
            flat.put("from", fromValue(item));
            flat.put("to", ProviderUtil.firstNonEmpty(Json.str(item, "inbox_email"), email));
            flat.put("subject", Json.str(item, "subject"));
            flat.put("text", Json.str(item, "body_text"));
            flat.put("html", Json.str(item, "body_html"));
            flat.put("date", Json.str(item, "received_at"));
            flat.put("read", Json.str(item, "is_read"));
            JsonArray attachments = Json.arr(item, "attachments");
            if (attachments != null) {
                flat.put("attachments", attachments);
            }
            result.add(Normalizer.normalizeEmail(flat, email));
        }
        return result;
    }
}
