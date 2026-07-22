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
 * disposablemail.com 渠道 — https://www.disposablemail.com
 *
 * <p>GET / 获取 session cookie + CSRF → GET /index/index 创建邮箱
 * → GET /index/refresh 获取邮件列表 → GET /email/id/{id} 获取正文。</p>
 */
public final class DisposablemailCom {

    private static final String BASE_URL = "https://www.disposablemail.com";
    private static final String TOK_PREFIX = "dmc1:";
    private static final Pattern CSRF_RE = Pattern.compile("CSRF=\"([^\"]+)\"");

    private DisposablemailCom() {
    }

    private static Map<String, String> ajaxHeaders(String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Referer", BASE_URL + "/");
        if (cookie != null && !cookie.isEmpty()) h.put("Cookie", cookie);
        return h;
    }

    private static String mergeCookies(String prev, HttpResult resp) {
        Map<String, String> jar = new LinkedHashMap<>();
        if (prev != null) {
            for (String part : prev.split(";")) {
                String p = part.trim();
                int eq = p.indexOf('=');
                if (eq > 0) jar.put(p.substring(0, eq).trim(), p.substring(eq + 1).trim());
            }
        }
        for (String sc : resp.getSetCookies()) {
            String seg = sc.split(";")[0].trim();
            int eq = seg.indexOf('=');
            if (eq > 0) jar.put(seg.substring(0, eq), seg.substring(eq + 1));
        }
        StringBuilder sb = new StringBuilder();
        for (var e : jar.entrySet()) {
            if (sb.length() > 0) sb.append("; ");
            sb.append(e.getKey()).append("=").append(e.getValue());
        }
        return sb.toString();
    }

    private static String encodeToken(String csrf, String cookie) {
        String raw = Json.serialize(Map.of("t", csrf, "c", cookie));
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.getBytes(StandardCharsets.UTF_8));
    }

    private static String[] decodeToken(String token) {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw new RuntimeException("disposablemail-com: 无效的 token");
        }
        String raw = new String(Base64.getDecoder().decode(token.substring(TOK_PREFIX.length())), StandardCharsets.UTF_8);
        JsonObject obj = Json.parseObject(raw);
        if (obj == null) throw new RuntimeException("disposablemail-com: 无效的 token");
        return new String[]{Json.str(obj, "t"), Json.str(obj, "c")};
    }

    /**
     * 创建 disposablemail.com 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> bh = new LinkedHashMap<>();
        bh.put("User-Agent", "Mozilla/5.0");
        bh.put("Accept", "text/html");
        HttpResult r1 = HttpClient.get(BASE_URL + "/", bh);
        r1.ensureSuccess();
        String cookie = mergeCookies("", r1);

        Matcher m = CSRF_RE.matcher(r1.getBody());
        if (!m.find()) throw new RuntimeException("disposablemail-com: 未能提取 CSRF token");
        String csrf = m.group(1);

        HttpResult r2 = HttpClient.get(
                BASE_URL + "/index/index?csrf_token=" + ProviderUtil.urlEncode(csrf),
                ajaxHeaders(cookie));
        r2.ensureSuccess();
        cookie = mergeCookies(cookie, r2);

        JsonObject data = Json.parseObject(r2.getBody());
        if (data == null) throw new RuntimeException("disposablemail-com: 创建邮箱响应格式异常");
        String email = Json.str(data, "email").trim();
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("disposablemail-com: 邮箱地址无效");
        }

        return new EmailInfo("disposablemail-com", email, encodeToken(csrf, cookie), null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 编码的 token
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String[] parts = decodeToken(token);
        String cookie = parts[1];

        HttpResult resp = HttpClient.get(BASE_URL + "/index/refresh", ajaxHeaders(cookie));
        resp.ensureSuccess();

        String trimmed = resp.getBody().trim();
        if ("0".equals(trimmed) || trimmed.isEmpty() || "[]".equals(trimmed)) {
            return new ArrayList<>();
        }

        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonArray()) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (!item.isJsonObject()) continue;
            JsonObject msg = item.getAsJsonObject();
            String mailId = Json.str(msg, "id");
            String htmlBody = fetchBody(cookie, mailId);

            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", mailId);
            row.put("from", Json.str(msg, "od"));
            row.put("to", email);
            row.put("subject", Json.str(msg, "predmet"));
            row.put("html", htmlBody);
            row.put("date", Json.str(msg, "kdy"));
            out.add(Normalizer.normalizeEmail(row, email));
        }
        return out;
    }

    private static String fetchBody(String cookie, String mailId) {
        if (mailId == null || mailId.isEmpty()) return "";
        try {
            HttpResult resp = HttpClient.get(BASE_URL + "/email/id/" + mailId, ajaxHeaders(cookie));
            return resp.isOk() ? resp.getBody() : "";
        } catch (RuntimeException e) {
            return "";
        }
    }
}
