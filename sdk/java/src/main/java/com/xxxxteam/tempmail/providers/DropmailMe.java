package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.nio.charset.StandardCharsets;
import java.time.LocalDateTime;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * dropmail-me 渠道 — https://dropmail.me
 * GraphQL 临时邮箱服务，需自行生成认证 token（FNV hash）。
 */
public final class DropmailMe {

    private static final String CHANNEL = "dropmail-me";
    private static final String BASE_URL = "https://dropmail.me";
    private static final Pattern DATA_K_RE = Pattern.compile(
            "<meta\\s+name=\"app-config\"\\s+data-k=\"([^\"]+)\"");

    private DropmailMe() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
        h.put("Accept", "application/json");
        h.put("Content-Type", "application/json");
        return h;
    }

    /** FNV-1a 变体哈希 */
    private static String fnvHash(String s) {
        long h = 2166136261L;
        for (int i = 0; i < s.length(); i++) {
            h ^= s.charAt(i);
            h += (h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24);
            h &= 0xFFFFFFFFL;
        }
        return Long.toHexString(h);
    }

    /** 生成 dropmail.me API 认证 token */
    private static String generateAuthToken() {
        // 获取页面，提取 data-k
        Map<String, String> pageHeaders = new LinkedHashMap<>();
        pageHeaders.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
        pageHeaders.put("Accept", "text/html");
        HttpResult resp = HttpClient.get(BASE_URL + "/en/", pageHeaders);
        resp.ensureSuccess();

        Matcher m = DATA_K_RE.matcher(resp.getBody());
        if (!m.find()) {
            throw new RuntimeException("dropmail-me: 无法从页面提取 data-k");
        }
        String dataK = m.group(1);

        // 反转 + base64 解码得到 secret
        String reversed = new StringBuilder(dataK).reverse().toString();
        String secret = new String(Base64.getDecoder().decode(reversed), StandardCharsets.UTF_8);

        // 生成随机部分
        String dateStr = LocalDateTime.now(ZoneOffset.UTC).format(DateTimeFormatter.ofPattern("yyyyMMdd"));
        String randomPart = dateStr + randomAlphanumeric(16);

        // 计算哈希
        String hashInput = randomPart + secret;
        String h = fnvHash(hashInput);

        return "website_" + randomPart + "_" + h;
    }

    private static String randomAlphanumeric(int len) {
        String chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        StringBuilder sb = new StringBuilder(len);
        ThreadLocalRandom r = ThreadLocalRandom.current();
        for (int i = 0; i < len; i++) {
            sb.append(chars.charAt(r.nextInt(chars.length())));
        }
        return sb.toString();
    }

    /**
     * 创建 dropmail.me 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String authToken = generateAuthToken();

        // 创建 session
        String query = "mutation { introduceSession { id expiresAt addresses { address } } }";
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("query", query);
        HttpResult resp = HttpClient.post(
                BASE_URL + "/api/graphql/" + ProviderUtil.urlEncode(authToken),
                Json.serialize(body), "application/json", headers());
        resp.ensureSuccess();

        JsonElement parsed = Json.parse(resp.getBody());
        JsonElement dataEl = parsed != null && parsed.isJsonObject() ? parsed.getAsJsonObject().get("data") : null;
        JsonElement sessionEl = dataEl != null && dataEl.isJsonObject()
                ? dataEl.getAsJsonObject().get("introduceSession") : null;
        if (sessionEl == null || !sessionEl.isJsonObject()) {
            throw new RuntimeException("dropmail-me: 创建 session 失败");
        }
        JsonObject session = sessionEl.getAsJsonObject();
        String sessionId = Json.str(session, "id");
        JsonElement addrsEl = session.get("addresses");
        if (sessionId.isEmpty() || addrsEl == null || !addrsEl.isJsonArray()
                || addrsEl.getAsJsonArray().isEmpty()) {
            throw new RuntimeException("dropmail-me: 响应中缺少 session ID 或地址");
        }
        String address = Json.str(addrsEl.getAsJsonArray().get(0), "address");
        if (address.isEmpty()) {
            throw new RuntimeException("dropmail-me: 地址为空");
        }

        // Token 序列化为 JSON
        Map<String, String> compositeToken = new LinkedHashMap<>();
        compositeToken.put("session_id", sessionId);
        compositeToken.put("auth_token", authToken);
        String tokenJson = Json.serialize(compositeToken);

        return new EmailInfo(CHANNEL, address, tokenJson, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 字符串 {"session_id":"...","auth_token":"..."}
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) {
            throw new IllegalArgumentException("dropmail-me: token 不能为空");
        }
        String addr = (email != null) ? email.trim() : "";
        if (addr.isEmpty()) {
            throw new IllegalArgumentException("dropmail-me: 邮箱地址不能为空");
        }

        // 解析复合 token
        JsonElement tokenParsed = Json.parse(token);
        if (tokenParsed == null || !tokenParsed.isJsonObject()) {
            throw new IllegalArgumentException("dropmail-me: token 格式无效");
        }
        String sessionId = Json.str(tokenParsed, "session_id");
        String authToken = Json.str(tokenParsed, "auth_token");
        if (sessionId.isEmpty() || authToken.isEmpty()) {
            throw new IllegalArgumentException("dropmail-me: token 中缺少 session_id 或 auth_token");
        }

        // 查询邮件
        String query = "{ session(id:\"" + sessionId + "\") "
                + "{ mails { id headerFrom headerSubject text html receivedAt } } }";
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("query", query);
        HttpResult resp = HttpClient.post(
                BASE_URL + "/api/graphql/" + ProviderUtil.urlEncode(authToken),
                Json.serialize(body), "application/json", headers());
        resp.ensureSuccess();

        JsonElement parsed = Json.parse(resp.getBody());
        JsonElement dataEl = parsed != null && parsed.isJsonObject() ? parsed.getAsJsonObject().get("data") : null;
        JsonElement sessionEl = dataEl != null && dataEl.isJsonObject()
                ? dataEl.getAsJsonObject().get("session") : null;
        if (sessionEl == null || !sessionEl.isJsonObject()) return new ArrayList<>();
        JsonElement mailsEl = sessionEl.getAsJsonObject().get("mails");
        if (mailsEl == null || !mailsEl.isJsonArray()) return new ArrayList<>();

        List<Email> result = new ArrayList<>();
        for (JsonElement mailEl : mailsEl.getAsJsonArray()) {
            if (!mailEl.isJsonObject()) continue;
            JsonObject msg = mailEl.getAsJsonObject();
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(msg, "id"));
            raw.put("from", Json.str(msg, "headerFrom"));
            raw.put("to", addr);
            raw.put("subject", Json.str(msg, "headerSubject"));
            raw.put("text", Json.str(msg, "text"));
            raw.put("html", Json.str(msg, "html"));
            raw.put("date", Json.str(msg, "receivedAt"));
            result.add(Normalizer.normalizeEmail(raw, addr));
        }
        return result;
    }
}
