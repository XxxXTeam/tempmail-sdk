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
 * Minuteinbox 渠道 — https://www.minuteinbox.com
 *
 * <p>GET / 获取 PHPSESSID cookie 和 CSRF → GET /index/index 创建邮箱
 * → GET /index/refresh 获取邮件列表 → POST /index/email 获取邮件详情。</p>
 */
public final class Minuteinbox {

    private static final String ORIGIN = "https://www.minuteinbox.com";
    private static final Pattern CSRF_RE = Pattern.compile("CSRF\\s*=\\s*\"([^\"]+)\"");

    private Minuteinbox() {
    }

    private static Map<String, String> ajaxHeaders(String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Referer", ORIGIN + "/");
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
     * 创建 minuteinbox 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> bh = new LinkedHashMap<>();
        bh.put("Accept", "text/html");
        bh.put("User-Agent", "Mozilla/5.0");
        HttpResult r1 = HttpClient.get(ORIGIN + "/", bh);
        r1.ensureSuccess();
        String cookie = mergeCookies("", r1);

        Matcher m = CSRF_RE.matcher(r1.getBody());
        if (!m.find()) throw new RuntimeException("minuteinbox: 无法提取 CSRF token");
        String csrf = m.group(1);

        HttpResult r2 = HttpClient.get(
                ORIGIN + "/index/index?csrf_token=" + ProviderUtil.urlEncode(csrf),
                ajaxHeaders(cookie));
        r2.ensureSuccess();
        cookie = mergeCookies(cookie, r2);

        JsonObject data = Json.parseObject(r2.getBody());
        if (data == null) throw new RuntimeException("minuteinbox: 响应格式异常");
        String email = Json.str(data, "email").trim();
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("minuteinbox: 返回的邮箱地址无效");
        }

        String token = Json.serialize(Map.of("csrf", csrf, "cookies", cookie));
        return new EmailInfo("minuteinbox", email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token JSON 编码的 {csrf, cookies}
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (token == null || token.isBlank()) throw new RuntimeException("minuteinbox: token 为空");
        if (email == null || email.isBlank()) throw new RuntimeException("minuteinbox: 邮箱地址为空");

        JsonObject tokObj = Json.parseObject(token);
        if (tokObj == null) throw new RuntimeException("minuteinbox: token 格式无效");
        String cookie = Json.str(tokObj, "cookies");

        HttpResult resp = HttpClient.get(ORIGIN + "/index/refresh", ajaxHeaders(cookie));
        resp.ensureSuccess();

        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonArray()) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (!item.isJsonObject()) continue;
            JsonObject row = item.getAsJsonObject();
            String mailId = Json.str(row, "id");

            // 获取邮件详情
            String bodyHtml = "";
            if (!mailId.isEmpty()) {
                try {
                    Map<String, String> dh = ajaxHeaders(cookie);
                    dh.put("Content-Type", "application/x-www-form-urlencoded");
                    HttpResult dr = HttpClient.post(ORIGIN + "/index/email",
                            "id=" + ProviderUtil.urlEncode(mailId),
                            "application/x-www-form-urlencoded", dh);
                    if (dr.isOk()) {
                        JsonObject detail = Json.parseObject(dr.getBody());
                        if (detail != null) bodyHtml = Json.str(detail, "telo");
                    }
                } catch (RuntimeException ignored) {
                }
            }

            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", mailId);
            flat.put("from", Json.str(row, "od"));
            flat.put("to", email);
            flat.put("subject", Json.str(row, "predmet"));
            flat.put("html", bodyHtml);
            flat.put("date", Json.str(row, "kdy"));
            out.add(Normalizer.normalizeEmail(flat, email));
        }
        return out;
    }
}
