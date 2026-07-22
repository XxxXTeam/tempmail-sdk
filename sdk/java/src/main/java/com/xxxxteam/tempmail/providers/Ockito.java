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
 * Ockito 渠道实现 — https://ockito.com
 *
 * <p>两段式认证：gtoken 获取 access/refresh token，后续请求需 Bearer 认证。
 * Token 以 JSON 格式序列化存储（含 access_token 和 refresh_token）。</p>
 */
public final class Ockito {

    private static final String BASE_URL = "https://ockito.com/web-api";

    private static final Map<String, String> DEFAULT_HEADERS = new LinkedHashMap<>();

    static {
        DEFAULT_HEADERS.put("Accept", "application/json");
        DEFAULT_HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36");
    }

    private Ockito() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        // 获取 token
        HttpResult loginResp = HttpClient.post(BASE_URL + "/gtoken", "{}", "application/json", DEFAULT_HEADERS);
        loginResp.ensureSuccess();
        JsonObject login = Json.parseObject(loginResp.getBody());
        if (login == null) {
            throw new RuntimeException("ockito: 无效的 token 响应");
        }
        String accessToken = Json.str(login, "access_token").trim();
        String refreshToken = Json.str(login, "refresh_token").trim();
        if (accessToken.isEmpty() || refreshToken.isEmpty()) {
            throw new RuntimeException("ockito: 无效的 token 响应");
        }

        // 获取邮箱地址
        Map<String, String> authHeaders = new LinkedHashMap<>(DEFAULT_HEADERS);
        authHeaders.put("Authorization", "Bearer " + accessToken);
        HttpResult emailResp = HttpClient.get(BASE_URL + "/email", authHeaders);
        emailResp.ensureSuccess();
        JsonObject emailData = Json.parseObject(emailResp.getBody());
        if (emailData == null) {
            throw new RuntimeException("ockito: 无效的邮箱响应");
        }
        String email = "";
        JsonElement emailEl = emailData.get("email");
        if (emailEl != null && emailEl.isJsonPrimitive()) {
            email = emailEl.getAsString().trim();
        } else if (emailEl != null && emailEl.isJsonObject()) {
            email = Json.str(emailEl, "email").trim();
        }
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("ockito: 无效的邮箱响应");
        }

        String packedToken = packToken(accessToken, refreshToken);
        return new EmailInfo("ockito", email, packedToken, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 序列化的 token（含 access_token 和 refresh_token）
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (token == null || token.isEmpty()) {
            throw new RuntimeException("ockito: 缺少 token");
        }
        String[] tokens = unpackToken(token);
        String accessToken = tokens[0];
        String refreshToken = tokens[1];
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("ockito: 缺少邮箱地址");
        }

        JsonObject data = fetchBearerJson(
                "/retrieve/" + ProviderUtil.urlEncode(address) + "/emails",
                accessToken, refreshToken);
        var rows = Json.arr(data, "inbox");
        List<Email> result = new ArrayList<>();
        if (rows == null) {
            return result;
        }
        for (JsonElement item : rows) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject row = item.getAsJsonObject();
            String uid = Json.str(row, "uid").trim();
            if (uid.isEmpty()) {
                result.add(Normalizer.normalizeEmail(flattenInboxRow(row, address), address));
                continue;
            }
            try {
                JsonObject detail = fetchBearerJson(
                        "/retrieve/" + ProviderUtil.urlEncode(address) + "/" + ProviderUtil.urlEncode(uid),
                        accessToken, refreshToken);
                result.add(Normalizer.normalizeEmail(flattenMessage(detail, address), address));
            } catch (RuntimeException e) {
                result.add(Normalizer.normalizeEmail(flattenInboxRow(row, address), address));
            }
        }
        return result;
    }

    private static String packToken(String accessToken, String refreshToken) {
        Map<String, Object> m = new LinkedHashMap<>();
        m.put("access_token", accessToken);
        m.put("refresh_token", refreshToken);
        return Json.serialize(m);
    }

    private static String[] unpackToken(String token) {
        String value = (token != null ? token : "").trim();
        if (value.isEmpty() || !value.startsWith("{")) {
            throw new RuntimeException("ockito: 无效的会话 token");
        }
        JsonObject obj = Json.parseObject(value);
        if (obj == null) {
            throw new RuntimeException("ockito: 无效的会话 token");
        }
        String access = Json.str(obj, "access_token").trim();
        String refresh = Json.str(obj, "refresh_token").trim();
        if (access.isEmpty() || refresh.isEmpty()) {
            throw new RuntimeException("ockito: 无效的会话 token");
        }
        return new String[]{access, refresh};
    }

    private static String refreshAccessToken(String refreshToken) {
        Map<String, String> headers = new LinkedHashMap<>(DEFAULT_HEADERS);
        headers.put("Authorization", "Bearer " + refreshToken);
        headers.put("X-PASSTHROUGH", "Y");
        HttpResult resp = HttpClient.post(BASE_URL + "/grefresh", "{}", "application/json", headers);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) {
            throw new RuntimeException("ockito: 刷新 token 失败");
        }
        String newAccess = Json.str(data, "access_token").trim();
        if (newAccess.isEmpty()) {
            throw new RuntimeException("ockito: 刷新 token 响应无效");
        }
        return newAccess;
    }

    private static JsonObject fetchBearerJson(String path, String accessToken, String refreshToken) {
        try {
            Map<String, String> headers = new LinkedHashMap<>(DEFAULT_HEADERS);
            headers.put("Authorization", "Bearer " + accessToken);
            HttpResult resp = HttpClient.get(BASE_URL + path, headers);
            if (resp.getStatusCode() == 401) {
                throw new RuntimeException("401");
            }
            resp.ensureSuccess();
            JsonObject obj = Json.parseObject(resp.getBody());
            return obj != null ? obj : new JsonObject();
        } catch (RuntimeException e) {
            if (!e.getMessage().contains("401")) {
                throw e;
            }
        }
        // 刷新后重试
        String refreshed = refreshAccessToken(refreshToken);
        Map<String, String> headers = new LinkedHashMap<>(DEFAULT_HEADERS);
        headers.put("Authorization", "Bearer " + refreshed);
        HttpResult resp = HttpClient.get(BASE_URL + path, headers);
        resp.ensureSuccess();
        JsonObject obj = Json.parseObject(resp.getBody());
        return obj != null ? obj : new JsonObject();
    }

    private static Map<String, Object> flattenInboxRow(JsonObject raw, String recipient) {
        Map<String, Object> dict = new LinkedHashMap<>();
        dict.put("id", Json.str(raw, "uid"));
        dict.put("from", Json.str(raw, "sender"));
        dict.put("to", recipient);
        dict.put("subject", Json.str(raw, "subject"));
        dict.put("text", Json.str(raw, "snippet"));
        dict.put("html", Json.str(raw, "html"));
        dict.put("date", Json.str(raw, "timestamp"));
        dict.put("is_read", false);
        return dict;
    }

    private static Map<String, Object> flattenMessage(JsonObject raw, String recipient) {
        JsonElement objEl = raw.get("obj");
        JsonObject obj = (objEl != null && objEl.isJsonObject()) ? objEl.getAsJsonObject() : raw;
        Map<String, Object> dict = new LinkedHashMap<>();
        dict.put("id", Json.str(raw, "uid"));
        dict.put("from", ProviderUtil.firstNonEmpty(
                Json.str(obj, "sender_email"), Json.str(obj, "SenderEmail"),
                Json.str(obj, "from_"), Json.str(obj, "From"),
                Json.str(obj, "from"), Json.str(obj, "sender_name"),
                Json.str(obj, "SenderName")));
        String to = ProviderUtil.firstNonEmpty(Json.str(obj, "to"), Json.str(obj, "To"));
        dict.put("to", to.isEmpty() ? recipient : to);
        dict.put("subject", ProviderUtil.firstNonEmpty(Json.str(obj, "subject"), Json.str(obj, "Subject")));
        dict.put("text", Json.str(obj, "text"));
        dict.put("html", Json.str(obj, "html"));
        dict.put("date", ProviderUtil.firstNonEmpty(Json.str(obj, "date"), Json.str(obj, "Date")));
        dict.put("is_read", false);
        return dict;
    }
}
