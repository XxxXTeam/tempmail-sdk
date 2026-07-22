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
 * TempGo Email 渠道 — https://tempgo.email
 * POST /api/generate 创建，GET /api/inbox?email={}&mailbox_id={} 收信。
 */
public final class TempGoEmail {

    private static final String BASE_URL = "https://tempgo.email";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
        HEADERS.put("Accept", "application/json");
    }

    private TempGoEmail() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 mailbox_id
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.post(BASE_URL + "/api/generate", null, null, HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String email = Json.str(data, "email").trim();
        String mailboxId = Json.str(data, "mailbox_id").trim();
        if (email.isEmpty() || mailboxId.isEmpty()) {
            throw new RuntimeException("tempgo-email: 创建邮箱响应无效");
        }
        return new EmailInfo("tempgo-email", email, mailboxId, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token mailbox_id
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String mailboxId = (token != null ? token : "").trim();
        String address = (email != null ? email : "").trim();
        if (mailboxId.isEmpty()) {
            throw new RuntimeException("tempgo-email: token 为空");
        }
        if (address.isEmpty()) {
            throw new RuntimeException("tempgo-email: 邮箱地址为空");
        }
        HttpResult resp = HttpClient.get(
                BASE_URL + "/api/inbox?email=" + ProviderUtil.urlEncode(address)
                        + "&mailbox_id=" + ProviderUtil.urlEncode(mailboxId), HEADERS);
        if (resp.getStatusCode() == 404) {
            return new ArrayList<>();
        }
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonArray messages = Json.arr(data, "messages");
        if (messages == null) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : messages) {
            if (item == null || !item.isJsonObject()) {
                continue;
            }
            Map<String, Object> raw = Json.toDict(item);
            raw.putIfAbsent("to", address);
            raw.putIfAbsent("from", raw.getOrDefault("sender", ""));
            raw.putIfAbsent("text", raw.getOrDefault("body_plain", ""));
            raw.putIfAbsent("html", raw.getOrDefault("body_html", ""));
            raw.putIfAbsent("date", raw.getOrDefault("received_at", ""));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }
}
