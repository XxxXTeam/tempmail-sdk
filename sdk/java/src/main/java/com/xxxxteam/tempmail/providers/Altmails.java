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
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Altmails 渠道 — https://tempmail.altmails.com
 *
 * <p>GET / 获取 session 和 CSRF → GET /random-email-address 获取随机邮箱
 * → POST /fetch-emails/{email} 获取邮件列表 → GET /view/{id} 获取正文。</p>
 */
public final class Altmails {

    private static final String BASE_URL = "https://tempmail.altmails.com";
    private static final Pattern CSRF_RE = Pattern.compile("['\"]_token['\"]\\s*:\\s*['\"]([^'\"]+)['\"]");

    private Altmails() {
    }

    private static Map<String, String> browserHeaders(String extra) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html,application/xhtml+xml");
        h.put("User-Agent", "Mozilla/5.0");
        return h;
    }

    private static String cookieStr(HttpResult resp) {
        StringBuilder sb = new StringBuilder();
        for (String sc : resp.getSetCookies()) {
            String seg = sc.split(";")[0].trim();
            if (sb.length() > 0) sb.append("; ");
            sb.append(seg);
        }
        return sb.toString();
    }

    private static String mergeCookieStr(String prev, HttpResult resp) {
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

    /**
     * 创建 altmails 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        // 获取首页，建立 session 并提取 CSRF
        HttpResult r1 = HttpClient.get(BASE_URL + "/", browserHeaders(null));
        r1.ensureSuccess();
        String cookies = cookieStr(r1);

        Matcher m = CSRF_RE.matcher(r1.getBody());
        if (!m.find()) throw new RuntimeException("altmails: 无法从首页提取 CSRF token");
        String csrf = m.group(1);

        // GET /random-email-address 获取随机邮箱地址
        Map<String, String> h2 = new LinkedHashMap<>();
        h2.put("Accept", "text/html");
        h2.put("User-Agent", "Mozilla/5.0");
        h2.put("Cookie", cookies);
        h2.put("Referer", BASE_URL + "/");
        HttpResult r2 = HttpClient.get(BASE_URL + "/random-email-address", h2);
        r2.ensureSuccess();

        cookies = mergeCookieStr(cookies, r2);
        String mailbox = r2.getBody().trim();
        if (mailbox.isEmpty() || !mailbox.contains("@")) {
            throw new RuntimeException("altmails: 返回的邮箱地址无效");
        }

        String token = Json.serialize(Map.of("csrf", csrf, "cookies", cookies));
        return new EmailInfo("altmails", mailbox, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token JSON 编码的 {csrf, cookies}
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (token == null || token.isBlank()) throw new RuntimeException("altmails: token 为空");
        if (email == null || email.isBlank()) throw new RuntimeException("altmails: 邮箱地址为空");

        JsonObject tokObj = Json.parseObject(token);
        if (tokObj == null) throw new RuntimeException("altmails: token 格式无效");
        String csrf = Json.str(tokObj, "csrf");
        String cookies = Json.str(tokObj, "cookies");

        // POST /fetch-emails/{email} 获取邮件列表
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Content-Type", "application/x-www-form-urlencoded");
        h.put("Cookie", cookies);
        h.put("Referer", BASE_URL + "/");
        h.put("User-Agent", "Mozilla/5.0");

        HttpResult resp = HttpClient.post(BASE_URL + "/fetch-emails/" + email,
                "_token=" + ProviderUtil.urlEncode(csrf),
                "application/x-www-form-urlencoded", h);
        resp.ensureSuccess();

        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonArray()) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (!item.isJsonObject()) continue;
            JsonObject msg = item.getAsJsonObject();
            String mailId = Json.str(msg, "id");

            // 尝试获取 HTML 正文
            String htmlBody = "";
            if (!mailId.isEmpty()) {
                try {
                    Map<String, String> vh = new LinkedHashMap<>();
                    vh.put("User-Agent", "Mozilla/5.0");
                    vh.put("Cookie", cookies);
                    vh.put("Referer", BASE_URL + "/");
                    HttpResult vr = HttpClient.get(BASE_URL + "/view/" + mailId, vh);
                    if (vr.isOk()) htmlBody = vr.getBody();
                } catch (RuntimeException ignored) {
                }
            }

            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", mailId);
            row.put("from", Json.str(msg, "from"));
            row.put("subject", Json.str(msg, "subject"));
            row.put("html", htmlBody);
            row.put("to", email);
            row.put("date", Json.str(msg, "date"));
            out.add(Normalizer.normalizeEmail(row, email));
        }
        return out;
    }
}
