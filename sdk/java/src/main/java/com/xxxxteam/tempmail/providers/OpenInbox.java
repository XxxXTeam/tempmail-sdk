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
 * OpenInbox 渠道 — https://openinbox.io
 *
 * <p>POST /api/inbox 创建收件箱，GET /api/emails/inbox/{id} 获取邮件列表，
 * GET /api/emails/{id} 获取邮件详情。</p>
 */
public final class OpenInbox {

    private static final String API_BASE = "https://api.openinbox.io/api";

    private OpenInbox() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json, text/plain, */*");
        h.put("Origin", "https://openinbox.io");
        h.put("Referer", "https://openinbox.io/");
        h.put("User-Agent", "Mozilla/5.0");
        return h;
    }

    /**
     * 创建 openinbox 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> h = headers();
        h.put("Content-Type", "application/json");
        HttpResult resp = HttpClient.post(API_BASE + "/inbox", null, "application/json", h);
        resp.ensureSuccess();

        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) throw new RuntimeException("openinbox: 响应格式无效");

        String inboxId = Json.str(data, "id").trim();
        String email = Json.str(data, "email").trim();
        if (inboxId.isEmpty() || email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("openinbox: 响应数据无效");
        }

        return new EmailInfo("openinbox", email, inboxId, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token inbox ID
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) throw new RuntimeException("openinbox: 空 inbox id");
        String inboxId = token.trim();

        HttpResult resp = HttpClient.get(
                API_BASE + "/emails/inbox/" + ProviderUtil.urlEncode(inboxId) + "?page=1&limit=50",
                headers());
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();
        var rows = Json.arr(body, "emails");
        if (rows == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : rows) {
            if (!item.isJsonObject()) continue;
            String msgId = Json.str(item.getAsJsonObject(), "id").trim();
            JsonObject detail = fetchDetail(msgId);
            JsonObject src = detail != null ? detail : item.getAsJsonObject();

            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", Json.str(src, "id"));
            flat.put("from", Json.str(src, "from"));
            flat.put("to", email);
            flat.put("subject", Json.str(src, "subject"));
            flat.put("text", Json.str(src, "textBody"));
            flat.put("html", Json.str(src, "htmlBody"));
            flat.put("receivedAt", Json.str(src, "receivedAt"));
            out.add(Normalizer.normalizeEmail(flat, email));
        }
        return out;
    }

    private static JsonObject fetchDetail(String messageId) {
        if (messageId == null || messageId.isEmpty()) return null;
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/emails/" + ProviderUtil.urlEncode(messageId), headers());
            if (!resp.isOk()) return null;
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException e) {
            return null;
        }
    }
}
