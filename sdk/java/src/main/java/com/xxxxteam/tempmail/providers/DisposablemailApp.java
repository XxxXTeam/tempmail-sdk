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
 * disposablemail.app 渠道 — https://disposablemail.app
 *
 * <p>POST /api/inbox 创建收件箱，GET /api/inbox/emails?token={token} 获取邮件。</p>
 */
public final class DisposablemailApp {

    private static final String BASE = "https://disposablemail.app";

    private DisposablemailApp() {
    }

    /**
     * 创建 disposablemail.app 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Content-Type", "application/json");
        h.put("Accept", "application/json");
        h.put("Referer", BASE + "/");
        h.put("Origin", BASE);

        HttpResult resp = HttpClient.post(BASE + "/api/inbox", "{}", "application/json", h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) throw new RuntimeException("disposablemail-app: 响应格式异常");

        String address = Json.str(body, "address");
        String token = Json.str(body, "token");
        if (address.isEmpty() || !address.contains("@")) {
            throw new RuntimeException("disposablemail-app: 返回的邮箱地址无效");
        }
        if (token.isEmpty()) throw new RuntimeException("disposablemail-app: 返回的 token 为空");

        return new EmailInfo("disposablemail-app", address, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token API token
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json");
        h.put("Referer", BASE + "/");

        HttpResult resp = HttpClient.get(
                BASE + "/api/inbox/emails?token=" + ProviderUtil.urlEncode(token), h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();

        var emails = Json.arr(body, "emails");
        if (emails == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : emails) {
            if (item.isJsonObject()) {
                out.add(Normalizer.normalizeEmail(Json.toDict(item), email));
            }
        }
        return out;
    }
}
