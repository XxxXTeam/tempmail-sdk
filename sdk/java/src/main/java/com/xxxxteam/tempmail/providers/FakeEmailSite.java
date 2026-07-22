package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
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
 * FakeEmailSite 渠道（fake-email.site）。
 * JSON REST API：创建临时邮箱 + 轮询收件箱。
 */
public final class FakeEmailSite {

    private static final String BASE = "https://api.fake-email.site";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("Content-Type", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    }

    private FakeEmailSite() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.post(BASE + "/api/temporary-address", "{}", "application/json", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("fake-email-site: 无效响应格式");
        }
        String email = Json.str(parsed, "temp_email_addr").trim();
        if (email.isEmpty()) {
            throw new RuntimeException("fake-email-site: 响应中未找到临时邮箱地址");
        }
        return new EmailInfo("fake-email-site", email, email, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("fake-email-site: empty email");
        }
        String addr = email.trim();
        Map<String, String> getHeaders = new LinkedHashMap<>(HEADERS);
        getHeaders.remove("Content-Type");
        HttpResult resp = HttpClient.get(
                BASE + "/api/inbox/poll?address=" + ProviderUtil.urlEncode(addr), getHeaders);
        if (resp.getStatusCode() == 404) {
            return new ArrayList<>();
        }
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonObject()) {
            return result;
        }
        JsonElement msgsEl = parsed.getAsJsonObject().get("messages");
        if (msgsEl == null || !msgsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement item : msgsEl.getAsJsonArray()) {
            if (item.isJsonObject()) {
                result.add(Normalizer.normalizeEmail(Json.toDict(item), addr));
            }
        }
        return result;
    }
}
