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
 * shitty.email 临时邮箱渠道。
 * POST /api/inbox 创建邮箱，GET /api/inbox 获取邮件列表，GET /api/email/{id} 获取详情。
 */
public final class ShittyEmail {

    private static final String API_BASE = "https://shitty.email/api";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0");
    }

    private ShittyEmail() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.post(API_BASE + "/inbox", null, null, HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String email = Json.str(data, "email").trim();
        String token = Json.str(data, "token").trim();
        if (email.isEmpty() || !email.contains("@") || token.isEmpty()) {
            throw new RuntimeException("shitty-email: 创建邮箱响应无效");
        }
        return new EmailInfo("shitty-email", email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token 会话令牌
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String sessionToken = (token != null ? token : "").trim();
        String address = (email != null ? email : "").trim();
        if (sessionToken.isEmpty()) {
            throw new RuntimeException("shitty-email: token 为空");
        }
        if (address.isEmpty()) {
            throw new RuntimeException("shitty-email: 邮箱地址为空");
        }
        Map<String, String> h = new LinkedHashMap<>(HEADERS);
        h.put("X-Session-Token", sessionToken);
        HttpResult resp = HttpClient.get(API_BASE + "/inbox", h);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonArray rows = Json.arr(data, "emails");
        if (rows == null) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : rows) {
            if (item == null || !item.isJsonObject()) {
                continue;
            }
            String msgId = Json.str(item, "id").trim();
            JsonElement detail = null;
            if (!msgId.isEmpty()) {
                detail = fetchDetail(sessionToken, msgId);
            }
            Map<String, Object> raw = Json.toDict(detail != null ? detail : item);
            raw.putIfAbsent("to", address);
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static JsonElement fetchDetail(String token, String messageId) {
        try {
            Map<String, String> h = new LinkedHashMap<>(HEADERS);
            h.put("X-Session-Token", token);
            HttpResult resp = HttpClient.get(
                    API_BASE + "/email/" + ProviderUtil.urlEncode(messageId), h);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parse(resp.getBody());
        } catch (Exception e) {
            return null;
        }
    }
}
