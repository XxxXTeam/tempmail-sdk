package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * WebMailTemp 渠道 — https://webmailtemp.com
 * GET /api/create 创建，GET /api/check/{username} 收信。
 * token 使用 base64 编码，前缀 "wmt1:"，内含 username 和 cookie。
 */
public final class WebMailTemp {

    private static final String BASE_URL = "https://webmailtemp.com";
    private static final String TOK_PREFIX = "wmt1:";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0");
    }

    private WebMailTemp() {
    }

    private static String encodeToken(String username, String cookie) {
        String raw = "{\"u\":\"" + username + "\",\"c\":\"" + cookie.replace("\"", "\\\"") + "\"}";
        return TOK_PREFIX + ProviderUtil.base64Encode(raw.getBytes(StandardCharsets.UTF_8));
    }

    private static String[] decodeToken(String token) {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw new RuntimeException("webmailtemp: 无效的 token");
        }
        byte[] decoded = ProviderUtil.base64Decode(token.substring(TOK_PREFIX.length()));
        JsonElement obj = Json.parse(new String(decoded, StandardCharsets.UTF_8));
        String username = Json.str(obj, "u").trim();
        String cookie = Json.str(obj, "c").trim();
        if (username.isEmpty() || cookie.isEmpty()) {
            throw new RuntimeException("webmailtemp: token 数据无效");
        }
        return new String[]{username, cookie};
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(BASE_URL + "/api/create", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonElement successEl = data.getAsJsonObject().get("success");
        if (successEl == null || !successEl.getAsBoolean()) {
            throw new RuntimeException("webmailtemp: 创建邮箱失败");
        }
        String address = Json.str(data, "email").trim();
        String username = Json.str(data, "username").trim();
        if (address.isEmpty() || !address.contains("@") || username.isEmpty()) {
            throw new RuntimeException("webmailtemp: 响应数据无效");
        }
        String cookie = ProviderUtil.extractAllCookies(resp);
        if (cookie.isEmpty()) {
            throw new RuntimeException("webmailtemp: 未获取到 cookie");
        }
        return new EmailInfo("webmailtemp", address, encodeToken(username, cookie), null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token base64 编码的认证信息
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("webmailtemp: 邮箱地址为空");
        }
        String[] parts = decodeToken(token);
        String username = parts[0];
        String cookie = parts[1];
        Map<String, String> h = new LinkedHashMap<>(HEADERS);
        h.put("Cookie", cookie);
        HttpResult resp = HttpClient.get(
                BASE_URL + "/api/check/" + ProviderUtil.urlEncode(username), h);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonArray emails = Json.arr(data, "emails");
        if (emails == null || emails.size() == 0) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : emails) {
            if (item == null || !item.isJsonObject()) continue;
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(item, "id"));
            raw.put("from", Json.str(item, "from"));
            raw.put("to", address);
            raw.put("subject", Json.str(item, "subject"));
            raw.put("text", Json.str(item, "body"));
            raw.put("html", Json.str(item, "html"));
            raw.put("date", ProviderUtil.firstNonEmpty(Json.str(item, "received_at"), Json.str(item, "timestamp")));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }
}
