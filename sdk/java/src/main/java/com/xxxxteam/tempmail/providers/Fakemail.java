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
 * FakeMail 渠道 — https://www.fakemail.net
 *
 * <p>GET / 获取 CSRF → GET /index/index 创建邮箱 → GET /index/refresh 获取邮件列表
 * → POST /index/email 获取邮件详情。</p>
 */
public final class Fakemail {

    private static final String BASE_URL = "https://www.fakemail.net";
    private static final Pattern CSRF_RE = Pattern.compile("const\\s+CSRF\\s*=\\s*\"([^\"]+)\"");

    private Fakemail() {
    }

    private static Map<String, String> ajaxHeaders(String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Referer", BASE_URL + "/");
        h.put("User-Agent", "Mozilla/5.0");
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

    /**
     * 创建 fakemail 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> bh = new LinkedHashMap<>();
        bh.put("Accept", "text/html,application/xhtml+xml");
        bh.put("User-Agent", "Mozilla/5.0");
        HttpResult r1 = HttpClient.get(BASE_URL + "/", bh);
        r1.ensureSuccess();

        Matcher m = CSRF_RE.matcher(r1.getBody());
        if (!m.find()) throw new RuntimeException("fakemail: csrf token not found");
        String csrf = m.group(1);
        String cookie = mergeCookies("", r1);

        HttpResult r2 = HttpClient.get(
                BASE_URL + "/index/index?csrf_token=" + ProviderUtil.urlEncode(csrf),
                ajaxHeaders(cookie));
        r2.ensureSuccess();
        cookie = mergeCookies(cookie, r2);

        JsonObject data = Json.parseObject(r2.getBody());
        if (data == null) throw new RuntimeException("fakemail: invalid mailbox response");
        String email = Json.str(data, "email").trim();
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("fakemail: invalid mailbox response");
        }

        return new EmailInfo("fakemail", email, cookie, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token session cookie
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) throw new RuntimeException("fakemail: empty session token");
        if (email == null || email.isBlank()) throw new RuntimeException("fakemail: empty email");

        HttpResult resp = HttpClient.get(BASE_URL + "/index/refresh", ajaxHeaders(token));
        resp.ensureSuccess();

        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonArray()) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (!item.isJsonObject()) continue;
            JsonObject row = item.getAsJsonObject();
            String msgId = Json.str(row, "id").trim();

            // 获取邮件详情
            JsonObject detail = null;
            if (!msgId.isEmpty()) {
                try {
                    Map<String, String> dh = ajaxHeaders(token);
                    dh.put("Content-Type", "application/x-www-form-urlencoded");
                    HttpResult dr = HttpClient.post(BASE_URL + "/index/email",
                            "id=" + ProviderUtil.urlEncode(msgId),
                            "application/x-www-form-urlencoded", dh);
                    if (dr.isOk()) detail = Json.parseObject(dr.getBody());
                } catch (RuntimeException ignored) {
                }
            }

            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", detail != null ? Json.str(detail, "id") : Json.str(row, "id"));
            flat.put("from", detail != null ? Json.str(detail, "od") : Json.str(row, "od"));
            flat.put("to", email);
            flat.put("subject", detail != null ? Json.str(detail, "predmet") : Json.str(row, "predmet"));
            flat.put("html", detail != null ? Json.str(detail, "telo") : "");
            flat.put("date", Json.str(row, "kdy"));
            out.add(Normalizer.normalizeEmail(flat, email));
        }
        return out;
    }
}
