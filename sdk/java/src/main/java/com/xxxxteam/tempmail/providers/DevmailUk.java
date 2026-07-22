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
 * DevMail UK 渠道（devmail.uk）。
 */
public final class DevmailUk {

    private static final String API_BASE = "https://www.devmail.uk/api";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
    }

    private DevmailUk() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(API_BASE + "/new", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("devmail-uk: invalid new email response");
        }
        String email = Json.str(parsed, "email").trim();
        if (email.isEmpty()) {
            String mailbox = Json.str(parsed, "mailbox").trim();
            if (!mailbox.isEmpty()) {
                email = mailbox + "@devmail.uk";
            }
        }
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("devmail-uk: invalid new email response");
        }
        return new EmailInfo("devmail-uk", email);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("devmail-uk: empty email");
        }
        String address = email.trim();
        String mailbox = address.contains("@") ? address.split("@", 2)[0].trim() : address;
        if (mailbox.isEmpty()) {
            throw new RuntimeException("devmail-uk: empty email");
        }
        HttpResult resp = HttpClient.get(
                API_BASE + "/inbox/" + ProviderUtil.urlEncode(mailbox) + "?detail=true", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonObject()) {
            return result;
        }
        JsonElement rowsEl = parsed.getAsJsonObject().get("emails");
        if (rowsEl == null || !rowsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement item : rowsEl.getAsJsonArray()) {
            if (item.isJsonObject()) {
                result.add(Normalizer.normalizeEmail(Json.toDict(item), address));
            }
        }
        return result;
    }
}
