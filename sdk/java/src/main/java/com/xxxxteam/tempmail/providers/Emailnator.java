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
 * Emailnator 渠道 — https://www.emailnator.com
 *
 * <p>需要 XSRF-TOKEN Session 认证。GET / 初始化 session →
 * POST /generate-email 创建邮箱 → POST /message-list 获取邮件。</p>
 */
public final class Emailnator {

    private static final String BASE_URL = "https://www.emailnator.com";

    private Emailnator() {
    }

    private static Map<String, String> defaultHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "application/json");
        h.put("Content-Type", "application/json");
        h.put("X-Requested-With", "XMLHttpRequest");
        return h;
    }

    /**
     * 初始化 session，获取 XSRF-TOKEN 和 Cookie。
     *
     * @return [xsrfToken, cookieStr]
     */
    private static String[] initSession() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "text/html");
        HttpResult resp = HttpClient.get(BASE_URL, h);
        resp.ensureSuccess();

        String xsrfToken = "";
        StringBuilder cookies = new StringBuilder();
        for (String sc : resp.getSetCookies()) {
            String seg = sc.split(";")[0].trim();
            if (cookies.length() > 0) cookies.append("; ");
            cookies.append(seg);
            // 提取 XSRF-TOKEN 值（URL 编码的）
            if (seg.startsWith("XSRF-TOKEN=")) {
                String encoded = seg.substring("XSRF-TOKEN=".length());
                xsrfToken = java.net.URLDecoder.decode(encoded, java.nio.charset.StandardCharsets.UTF_8);
            }
        }
        if (xsrfToken.isEmpty()) {
            throw new RuntimeException("emailnator: 无法提取 XSRF-TOKEN");
        }
        return new String[]{xsrfToken, cookies.toString()};
    }

    /**
     * 创建 emailnator 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String[] session = initSession();
        String xsrfToken = session[0];
        String cookies = session[1];

        Map<String, String> h = defaultHeaders();
        h.put("X-XSRF-TOKEN", xsrfToken);
        h.put("Cookie", cookies);

        String payload = Json.serialize(Map.of("email", List.of("plusGmail", "dotGmail", "googleMail")));
        HttpResult resp = HttpClient.post(BASE_URL + "/generate-email", payload, "application/json", h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) throw new RuntimeException("emailnator: 响应格式异常");
        var emailArr = Json.arr(body, "email");
        if (emailArr == null || emailArr.isEmpty()) {
            throw new RuntimeException("emailnator: 响应中无邮箱地址");
        }
        String email = emailArr.get(0).getAsString();

        String token = Json.serialize(Map.of("xsrfToken", xsrfToken, "cookies", cookies));
        return new EmailInfo("emailnator", email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 编码的 {xsrfToken, cookies}
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) throw new RuntimeException("emailnator: token 为空");

        JsonObject tok = Json.parseObject(token);
        if (tok == null) throw new RuntimeException("emailnator: token 格式无效");
        String xsrfToken = Json.str(tok, "xsrfToken");
        String cookies = Json.str(tok, "cookies");

        Map<String, String> h = defaultHeaders();
        h.put("X-XSRF-TOKEN", xsrfToken);
        h.put("Cookie", cookies);

        String payload = Json.serialize(Map.of("email", email));
        HttpResult resp = HttpClient.post(BASE_URL + "/message-list", payload, "application/json", h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();
        var messageData = Json.arr(body, "messageData");
        if (messageData == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : messageData) {
            if (!item.isJsonObject()) continue;
            JsonObject msg = item.getAsJsonObject();
            String msgId = Json.str(msg, "messageID");
            // 过滤广告消息
            if (msgId.startsWith("ADS")) continue;

            // 获取邮件详情
            String html = "";
            if (!msgId.isEmpty()) {
                try {
                    String detPayload = Json.serialize(Map.of("email", email, "messageID", msgId));
                    HttpResult dr = HttpClient.post(BASE_URL + "/message-list",
                            detPayload, "application/json", h);
                    if (dr.isOk()) html = dr.getBody();
                } catch (RuntimeException ignored) {
                }
            }

            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", msgId);
            row.put("from", Json.str(msg, "from"));
            row.put("to", email);
            row.put("subject", Json.str(msg, "subject"));
            row.put("html", html);
            row.put("date", Json.str(msg, "time"));
            out.add(Normalizer.normalizeEmail(row, email));
        }
        return out;
    }
}
