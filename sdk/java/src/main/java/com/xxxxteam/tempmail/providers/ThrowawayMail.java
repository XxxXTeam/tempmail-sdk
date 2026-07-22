package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
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
 * ThrowawayMail 渠道 — https://throwawaymail.app
 * POST /api/mailboxes 创建，GET /api/mailboxes/{id}/messages 收信。
 */
public final class ThrowawayMail {

    private static final String API_BASE = "https://throwawaymail.app/api";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
    }

    private ThrowawayMail() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.post(API_BASE + "/mailboxes", null, null, HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String mailboxId = Json.str(data, "mailbox_id").trim();
        String email = Json.str(data, "address").trim();
        if (mailboxId.isEmpty() || email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("throwawaymail: 创建邮箱响应无效");
        }
        return new EmailInfo("throwawaymail", email, mailboxId, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token 邮箱 mailbox_id
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String mailboxId = (token != null ? token : "").trim();
        String address = (email != null ? email : "").trim();
        if (mailboxId.isEmpty()) {
            throw new RuntimeException("throwawaymail: mailbox_id 为空");
        }
        if (address.isEmpty()) {
            throw new RuntimeException("throwawaymail: 邮箱地址为空");
        }
        HttpResult resp = HttpClient.get(
                API_BASE + "/mailboxes/" + ProviderUtil.urlEncode(mailboxId) + "/messages", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonArray()) {
            return result;
        }
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (item == null || !item.isJsonObject()) {
                continue;
            }
            String msgId = Json.str(item, "message_id").trim();
            JsonElement detail = null;
            if (!msgId.isEmpty()) {
                detail = fetchDetail(mailboxId, msgId);
            }
            Map<String, Object> raw = buildRaw(detail != null ? detail : item, address);
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static JsonElement fetchDetail(String mailboxId, String messageId) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/mailboxes/" + ProviderUtil.urlEncode(mailboxId)
                            + "/messages/" + ProviderUtil.urlEncode(messageId), HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parse(resp.getBody());
        } catch (Exception e) {
            return null;
        }
    }

    private static Map<String, Object> buildRaw(JsonElement el, String recipient) {
        Map<String, Object> raw = Json.toDict(el);
        raw.put("id", raw.getOrDefault("message_id", ""));
        raw.putIfAbsent("to", recipient);
        return raw;
    }
}
