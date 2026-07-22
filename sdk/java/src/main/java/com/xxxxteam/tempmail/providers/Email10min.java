package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * email10min 渠道 — https://email10min.com
 *
 * <p>GET /zh 获取 CSRF + Cookie + 邮箱地址，POST /messages 获取邮件。</p>
 */
public final class Email10min {

    private static final String BASE_URL = "https://email10min.com";
    private static final String TOK_PREFIX = "e10m:";
    private static final Pattern CSRF_META_RE = Pattern.compile(
            "<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"", Pattern.CASE_INSENSITIVE);
    private static final Pattern EMAIL_ID_RE = Pattern.compile(
            "id=\"emailAddress\"[^>]*>([^<]+)", Pattern.CASE_INSENSITIVE);
    private static final Pattern EMAIL_DATA_RE = Pattern.compile(
            "data-email=\"([^\"]+@[^\"]+)\"", Pattern.CASE_INSENSITIVE);
    private static final Pattern EMAIL_JSON_RE = Pattern.compile(
            "\"mailbox\"\\s*:\\s*\"([^\"]+@[^\"]+)\"");
    private static final Pattern EMAIL_GENERIC_RE = Pattern.compile(
            "([a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,})");

    private Email10min() {
    }

    private static String cookieFromResp(HttpResult resp) {
        StringBuilder sb = new StringBuilder();
        for (String sc : resp.getSetCookies()) {
            String seg = sc.split(";")[0].trim();
            if (sb.length() > 0) sb.append("; ");
            sb.append(seg);
        }
        return sb.toString();
    }

    private static String encodeToken(String cookie, String csrf) {
        String raw = Json.serialize(Map.of("c", cookie, "t", csrf));
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.getBytes(StandardCharsets.UTF_8));
    }

    private static String[] decodeToken(String token) {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw new RuntimeException("email10min: invalid session token");
        }
        String raw = new String(Base64.getDecoder().decode(token.substring(TOK_PREFIX.length())), StandardCharsets.UTF_8);
        JsonObject obj = Json.parseObject(raw);
        if (obj == null) throw new RuntimeException("email10min: invalid session token");
        String cookie = Json.str(obj, "c").trim();
        String csrf = Json.str(obj, "t").trim();
        if (cookie.isEmpty() || csrf.isEmpty()) {
            throw new RuntimeException("email10min: invalid session token (empty fields)");
        }
        return new String[]{cookie, csrf};
    }

    private static String extractEmail(String html) {
        Matcher m = EMAIL_ID_RE.matcher(html);
        if (m.find() && m.group(1).contains("@")) return m.group(1).trim();
        m = EMAIL_DATA_RE.matcher(html);
        if (m.find()) return m.group(1).trim();
        m = EMAIL_JSON_RE.matcher(html);
        if (m.find()) return m.group(1).trim();
        m = EMAIL_GENERIC_RE.matcher(html);
        if (m.find()) {
            String addr = m.group(1);
            if (!addr.contains("email10min") && !addr.contains("example") && !addr.contains("google")) {
                return addr.trim();
            }
        }
        throw new RuntimeException("email10min: 未找到邮箱地址");
    }

    /**
     * 创建 email10min 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html");
        h.put("User-Agent", "Mozilla/5.0");
        HttpResult resp = HttpClient.get(BASE_URL + "/zh", h);
        resp.ensureSuccess();

        String cookie = cookieFromResp(resp);
        String html = resp.getBody();

        Matcher csrfM = CSRF_META_RE.matcher(html);
        if (!csrfM.find()) throw new RuntimeException("email10min: 未找到 CSRF token");
        String csrf = csrfM.group(1);

        String emailAddr = extractEmail(html);
        String token = encodeToken(cookie, csrf);
        return new EmailInfo("email10min", emailAddr, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 编码的 session token
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String[] parts = decodeToken(token);
        String cookie = parts[0];
        String csrf = parts[1];

        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json, text/plain, */*");
        h.put("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
        h.put("Origin", BASE_URL);
        h.put("Referer", BASE_URL + "/zh");
        h.put("User-Agent", "Mozilla/5.0");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Cookie", cookie);

        String ts = String.valueOf(System.currentTimeMillis());
        HttpResult resp = HttpClient.post(BASE_URL + "/messages?" + ts,
                "_token=" + ProviderUtil.urlEncode(csrf) + "&captcha=",
                "application/x-www-form-urlencoded; charset=UTF-8", h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();
        var messages = Json.arr(body, "messages");
        if (messages == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : messages) {
            if (!item.isJsonObject()) continue;
            out.add(Normalizer.normalizeEmail(Json.toDict(item), email));
        }
        return out;
    }
}
