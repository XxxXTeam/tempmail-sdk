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
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.nio.charset.StandardCharsets;

/**
 * mail.zhujump.com 固定域渠道。
 *
 * <p>通过注册账号、登录会话后创建固定域邮箱，再通过邮箱 ID 拉取邮件列表。
 * 渠道别名：jqkjqk-xyz / lyhlevi-com</p>
 */
public final class Zhujump {

    private static final String BASE_URL = "https://mail.zhujump.com";
    private static final String TOKEN_PREFIX = "zhj1:";
    private static final long DEFAULT_EXPIRY_TIME = 60 * 60 * 1000L;

    private Zhujump() {
    }

    private static Map<String, String> baseHeaders(String baseUrl) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json");
        h.put("Origin", baseUrl);
        h.put("Referer", baseUrl + "/zh-CN/login");
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
        return h;
    }

    /**
     * 创建临时邮箱（默认实例 mail.zhujump.com）。
     *
     * @param domain  邮箱域名
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain, String channel) {
        return generateForInstance(BASE_URL, domain, channel, DEFAULT_EXPIRY_TIME);
    }

    /**
     * 创建临时邮箱（指定实例）。
     *
     * @param baseUrl    实例基地址
     * @param domain     邮箱域名
     * @param channel    渠道标识
     * @param expiryTime 过期时间（毫秒），null 表示不设过期
     * @return 邮箱信息
     */
    public static EmailInfo generateForInstance(String baseUrl, String domain, String channel, Long expiryTime) {
        baseUrl = baseUrl.replaceAll("/+$", "");
        Map<String, String> headers = baseHeaders(baseUrl);

        String username = "sdk" + ProviderUtil.randomString(10);
        String password = "Sdk!" + ProviderUtil.randomString(12) + "A1";

        // 1. 注册账号
        Map<String, Object> regBody = new LinkedHashMap<>();
        regBody.put("username", username);
        regBody.put("password", password);
        regBody.put("turnstileToken", "");
        HttpResult regResp = HttpClient.post(baseUrl + "/api/auth/register",
                Json.serialize(regBody), "application/json", headers);
        regResp.ensureSuccess();

        // 2. 获取 CSRF token
        HttpResult csrfResp = HttpClient.get(baseUrl + "/api/auth/csrf", headers);
        csrfResp.ensureSuccess();
        JsonObject csrfData = Json.parseObject(csrfResp.getBody());
        String csrf = csrfData != null ? Json.str(csrfData, "csrfToken").trim() : "";
        if (csrf.isEmpty()) {
            throw new RuntimeException("zhujump: csrf token missing");
        }

        // 3. 登录（form-urlencoded，不跟随重定向以捕获 Set-Cookie）
        String loginBody = "username=" + ProviderUtil.urlEncode(username)
                + "&password=" + ProviderUtil.urlEncode(password)
                + "&turnstileToken="
                + "&redirect=false"
                + "&csrfToken=" + ProviderUtil.urlEncode(csrf)
                + "&callbackUrl=" + ProviderUtil.urlEncode(baseUrl + "/zh-CN/login");
        Map<String, String> loginHeaders = new LinkedHashMap<>(headers);
        loginHeaders.put("x-auth-return-redirect", "1");
        loginHeaders.put("Content-Type", "application/x-www-form-urlencoded");
        HttpResult loginResp = HttpClient.postNoRedirect(
                baseUrl + "/api/auth/callback/credentials?",
                loginBody, "application/x-www-form-urlencoded", loginHeaders);
        // 登录可能返回 2xx 或 3xx

        // 收集 Cookie
        String cookie = collectCookies(regResp, csrfResp, loginResp);

        // 4. 验证 session
        Map<String, String> sessionHeaders = new LinkedHashMap<>(headers);
        sessionHeaders.put("Cookie", cookie);
        HttpResult sessionResp = HttpClient.get(baseUrl + "/api/auth/session", sessionHeaders);
        sessionResp.ensureSuccess();
        // 合并 session 响应的 Cookie
        cookie = mergeCookies(cookie, sessionResp);

        // 5. 创建邮箱
        Map<String, Object> createBody = new LinkedHashMap<>();
        createBody.put("name", "sdk" + ProviderUtil.randomString(10));
        createBody.put("domain", domain);
        if (expiryTime != null) {
            createBody.put("expiryTime", expiryTime);
        }
        Map<String, String> createHeaders = new LinkedHashMap<>(headers);
        createHeaders.put("Cookie", cookie);
        HttpResult createResp = HttpClient.post(baseUrl + "/api/emails/generate",
                Json.serialize(createBody), "application/json", createHeaders);
        createResp.ensureSuccess();
        JsonObject created = Json.parseObject(createResp.getBody());
        if (created == null) {
            throw new RuntimeException("zhujump: invalid generate response");
        }
        String email = Json.str(created, "email").trim();
        String emailId = Json.str(created, "id").trim();
        if (email.isEmpty() || emailId.isEmpty()) {
            throw new RuntimeException("zhujump: invalid generate response");
        }

        String token = encodeToken(cookie, emailId, baseUrl);
        return new EmailInfo(channel, email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token 编码后的会话令牌
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        Map<String, String> session = decodeToken(token);
        String baseUrl = session.get("base_url");
        String emailId = session.get("email_id");
        String cookie = session.get("cookie");

        Map<String, String> headers = baseHeaders(baseUrl);
        headers.put("Cookie", cookie);

        HttpResult resp = HttpClient.get(baseUrl + "/api/emails/" + ProviderUtil.urlEncode(emailId), headers);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) {
            return new ArrayList<>();
        }
        JsonArray rows = Json.arr(data, "messages");
        if (rows == null) {
            return new ArrayList<>();
        }

        List<Email> result = new ArrayList<>();
        for (JsonElement row : rows) {
            if (!row.isJsonObject()) {
                continue;
            }
            JsonObject item = row.getAsJsonObject();
            JsonObject source = item;
            String messageId = Json.str(item, "id").trim();
            // 如果缺少正文则尝试拉取详情
            if (!messageId.isEmpty()
                    && Json.str(item, "content").trim().isEmpty()
                    && Json.str(item, "html").trim().isEmpty()) {
                JsonObject detail = fetchDetail(baseUrl, cookie, emailId, messageId);
                if (detail != null) {
                    source = mergeJson(item, detail);
                }
            }
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", Json.str(source, "id"));
            flat.put("from_address", Json.str(source, "from_address"));
            flat.put("to_address", ProviderUtil.firstNonEmpty(Json.str(source, "to_address"), email));
            flat.put("subject", Json.str(source, "subject"));
            flat.put("content", Json.str(source, "content"));
            flat.put("html", Json.str(source, "html"));
            flat.put("received_at", ProviderUtil.firstNonEmpty(
                    Json.str(source, "received_at"), Json.str(source, "sent_at")));
            flat.put("isRead", false);
            result.add(Normalizer.normalizeEmail(flat, email));
        }
        return result;
    }

    private static JsonObject fetchDetail(String baseUrl, String cookie, String emailId, String messageId) {
        try {
            Map<String, String> headers = baseHeaders(baseUrl);
            headers.put("Cookie", cookie);
            HttpResult resp = HttpClient.get(
                    baseUrl + "/api/emails/" + ProviderUtil.urlEncode(emailId)
                            + "/" + ProviderUtil.urlEncode(messageId), headers);
            if (!resp.isOk()) {
                return null;
            }
            JsonObject data = Json.parseObject(resp.getBody());
            if (data == null) {
                return null;
            }
            // 实际详情在 "message" 字段中
            if (data.has("message") && data.get("message").isJsonObject()) {
                return data.getAsJsonObject("message");
            }
            return data;
        } catch (RuntimeException ignored) {
            return null;
        }
    }

    private static JsonObject mergeJson(JsonObject base, JsonObject overlay) {
        JsonObject result = base.deepCopy();
        for (Map.Entry<String, JsonElement> entry : overlay.entrySet()) {
            if (!result.has(entry.getKey()) || result.get(entry.getKey()).isJsonNull()
                    || (result.get(entry.getKey()).isJsonPrimitive()
                    && result.get(entry.getKey()).getAsString().isEmpty())) {
                result.add(entry.getKey(), entry.getValue());
            }
        }
        return result;
    }

    private static String encodeToken(String cookie, String emailId, String baseUrl) {
        Map<String, Object> data = new LinkedHashMap<>();
        data.put("c", cookie);
        data.put("i", emailId);
        data.put("b", baseUrl);
        String json = Json.serialize(data);
        return TOKEN_PREFIX + Base64.getEncoder().encodeToString(json.getBytes(StandardCharsets.UTF_8));
    }

    private static Map<String, String> decodeToken(String token) {
        if (token == null || !token.startsWith(TOKEN_PREFIX)) {
            throw new IllegalArgumentException("zhujump: invalid session token");
        }
        try {
            byte[] decoded = Base64.getDecoder().decode(token.substring(TOKEN_PREFIX.length()));
            JsonObject data = Json.parseObject(new String(decoded, StandardCharsets.UTF_8));
            if (data == null) {
                throw new IllegalArgumentException("zhujump: invalid session token");
            }
            String cookie = Json.str(data, "c").trim();
            String emailId = Json.str(data, "i").trim();
            if (cookie.isEmpty() || emailId.isEmpty()) {
                throw new IllegalArgumentException("zhujump: invalid session token");
            }
            String baseUrl = Json.str(data, "b").trim();
            if (baseUrl.isEmpty()) {
                baseUrl = BASE_URL;
            }
            baseUrl = baseUrl.replaceAll("/+$", "");
            Map<String, String> result = new LinkedHashMap<>();
            result.put("cookie", cookie);
            result.put("email_id", emailId);
            result.put("base_url", baseUrl);
            return result;
        } catch (IllegalArgumentException e) {
            throw e;
        } catch (RuntimeException e) {
            throw new IllegalArgumentException("zhujump: invalid session token", e);
        }
    }

    /**
     * 收集多个响应中的 Set-Cookie 并拼接为 Cookie 头字符串。
     */
    private static String collectCookies(HttpResult... responses) {
        Map<String, String> cookies = new LinkedHashMap<>();
        for (HttpResult resp : responses) {
            if (resp == null) {
                continue;
            }
            for (String sc : resp.getSetCookies()) {
                String[] parts = sc.split(";", 2);
                String nameValue = parts[0].trim();
                int eq = nameValue.indexOf('=');
                if (eq > 0) {
                    cookies.put(nameValue.substring(0, eq), nameValue.substring(eq + 1));
                }
            }
        }
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> kv : cookies.entrySet()) {
            if (sb.length() > 0) {
                sb.append("; ");
            }
            sb.append(kv.getKey()).append("=").append(kv.getValue());
        }
        return sb.toString();
    }

    /**
     * 将已有 cookie 字符串与新响应的 Set-Cookie 合并。
     */
    private static String mergeCookies(String existing, HttpResult resp) {
        Map<String, String> cookies = new LinkedHashMap<>();
        // 解析已有
        if (existing != null && !existing.isEmpty()) {
            for (String pair : existing.split(";")) {
                String p = pair.trim();
                int eq = p.indexOf('=');
                if (eq > 0) {
                    cookies.put(p.substring(0, eq), p.substring(eq + 1));
                }
            }
        }
        // 合并新 Set-Cookie
        if (resp != null) {
            for (String sc : resp.getSetCookies()) {
                String[] parts = sc.split(";", 2);
                String nameValue = parts[0].trim();
                int eq = nameValue.indexOf('=');
                if (eq > 0) {
                    cookies.put(nameValue.substring(0, eq), nameValue.substring(eq + 1));
                }
            }
        }
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> kv : cookies.entrySet()) {
            if (sb.length() > 0) {
                sb.append("; ");
            }
            sb.append(kv.getKey()).append("=").append(kv.getValue());
        }
        return sb.toString();
    }
}