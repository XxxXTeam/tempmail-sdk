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
 * mailcat.ai 渠道 — https://mailcat.ai
 *
 * <p>POST /mailboxes 创建邮箱（无 body），返回 Bearer token。
 * GET /inbox 获取邮件列表。</p>
 */
public final class MailcatAi {

    private static final String BASE_URL = "https://api.mailcat.ai";

    private MailcatAi() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "application/json");
        return h;
    }

    /**
     * 创建 mailcat.ai 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.post(BASE_URL + "/mailboxes", null, null, headers());
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) throw new RuntimeException("mailcat-ai: 响应格式无效");

        JsonObject data = body.has("data") && body.get("data").isJsonObject()
                ? body.getAsJsonObject("data") : null;
        if (data == null) throw new RuntimeException("mailcat-ai: 响应 data 格式无效");

        String address = Json.str(data, "email");
        String token = Json.str(data, "token");
        if (address.isEmpty()) throw new RuntimeException("mailcat-ai: 缺少邮箱地址");
        if (token.isEmpty()) throw new RuntimeException("mailcat-ai: 缺少认证令牌");

        return new EmailInfo("mailcat-ai", address, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token Bearer token
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        Map<String, String> h = headers();
        h.put("Authorization", "Bearer " + (token != null ? token : ""));

        HttpResult resp = HttpClient.get(BASE_URL + "/inbox", h);
        if (resp.getStatusCode() == 404) return new ArrayList<>();
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();

        var messages = Json.arr(body, "data");
        if (messages == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : messages) {
            if (item.isJsonObject()) {
                out.add(Normalizer.normalizeEmail(Json.toDict(item), email));
            }
        }
        return out;
    }
}
