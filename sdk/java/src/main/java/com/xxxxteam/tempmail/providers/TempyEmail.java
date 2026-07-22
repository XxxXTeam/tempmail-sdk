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
 * Tempy Email 渠道实现 — https://tempy.email
 */
public final class TempyEmail {

    private static final String API_BASE = "https://tempy.email/api/v1";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();
    private static final Map<String, String> POST_HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36");
        POST_HEADERS.putAll(HEADERS);
        POST_HEADERS.put("Content-Type", "application/json");
    }

    private TempyEmail() {
    }

    /**
     * 创建临时邮箱（支持指定域名）。
     *
     * @param domain 指定域名，null 或空串则由服务端分配
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        Map<String, Object> body = new LinkedHashMap<>();
        if (domain != null && !domain.trim().isEmpty()) {
            body.put("domain", domain.trim());
        }
        String payload = Json.serialize(body);
        HttpResult resp = HttpClient.post(API_BASE + "/mailbox", payload, "application/json", POST_HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonObject()) {
            throw new RuntimeException("tempy-email: 无效的创建响应");
        }
        String email = Json.str(data, "email").trim();
        if (email.isEmpty()) {
            throw new RuntimeException("tempy-email: 无效的创建响应");
        }
        String expiresAt = Json.str(data, "expires_at");
        return new EmailInfo("tempy-email", email, null,
                expiresAt.isEmpty() ? null : parseLong(expiresAt), null);
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
            throw new RuntimeException("tempy-email: 邮箱地址为空");
        }
        HttpResult resp = HttpClient.get(
                API_BASE + "/mailbox/" + ProviderUtil.urlEncode(address) + "/messages", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        JsonArray rows = null;
        if (data != null && data.isJsonObject()) {
            rows = Json.arr(data, "messages");
        } else if (data != null && data.isJsonArray()) {
            rows = data.getAsJsonArray();
        }
        if (rows == null) {
            return result;
        }
        for (JsonElement item : rows) {
            if (!item.isJsonObject()) {
                continue;
            }
            result.add(Normalizer.normalizeEmail(flattenMessage(item.getAsJsonObject(), address), address));
        }
        return result;
    }

    private static Map<String, Object> flattenMessage(JsonObject raw, String recipient) {
        Map<String, Object> dict = new LinkedHashMap<>();
        dict.put("id", ProviderUtil.firstNonEmpty(
                Json.str(raw, "id"), Json.str(raw, "messageId"),
                Json.str(raw, "message_id"), Json.str(raw, "mail_id")));
        dict.put("from", ProviderUtil.firstNonEmpty(
                Json.str(raw, "from"), Json.str(raw, "sender"),
                Json.str(raw, "from_addr"), Json.str(raw, "from_address")));
        String to = Json.str(raw, "to");
        dict.put("to", to.isEmpty() ? recipient : to);
        dict.put("subject", ProviderUtil.firstNonEmpty(Json.str(raw, "subject"), Json.str(raw, "mail_title")));
        dict.put("text", ProviderUtil.firstNonEmpty(
                Json.str(raw, "text"), Json.str(raw, "body_text"),
                Json.str(raw, "text_body"), Json.str(raw, "body")));
        dict.put("html", ProviderUtil.firstNonEmpty(
                Json.str(raw, "html"), Json.str(raw, "body_html"), Json.str(raw, "html_body")));
        dict.put("date", ProviderUtil.firstNonEmpty(
                Json.str(raw, "date"), Json.str(raw, "received_at"), Json.str(raw, "created_at")));
        return dict;
    }

    private static Long parseLong(String s) {
        try {
            return Long.parseLong(s);
        } catch (NumberFormatException e) {
            return null;
        }
    }
}
