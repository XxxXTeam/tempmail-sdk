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
 * Mailgolem 渠道 — https://mailgolem.com
 *
 * <p>GET / 获取 session cookie 和 CSRF → GET /random-email-address 获取邮箱
 * → POST /fetch-emails/{email} 获取邮件列表。</p>
 */
public final class Mailgolem {

    private static final String BASE = "https://mailgolem.com";
    private static final Pattern CSRF_RE = Pattern.compile(
            "<input[^>]+name=\"_token\"[^>]+value=\"([^\"]+)\"");

    private Mailgolem() {
    }

    private static Map<String, String> defaultHeaders() {
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

    /**
     * 创建 mailgolem 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult r1 = HttpClient.get(BASE + "/", defaultHeaders());
        r1.ensureSuccess();
        String cookies = cookieStr(r1);

        Matcher m = CSRF_RE.matcher(r1.getBody());
        if (!m.find()) throw new RuntimeException("mailgolem: 无法提取 CSRF token");
        String csrf = m.group(1);

        Map<String, String> h2 = defaultHeaders();
        h2.put("Referer", BASE + "/");
        h2.put("Cookie", cookies);
        HttpResult r2 = HttpClient.get(BASE + "/random-email-address", h2);
        r2.ensureSuccess();

        // 合并 cookie
        String moreCookies = cookieStr(r2);
        if (!moreCookies.isEmpty()) {
            cookies = cookies.isEmpty() ? moreCookies : cookies + "; " + moreCookies;
        }

        String emailAddr = r2.getBody().trim();
        if (emailAddr.isEmpty() || !emailAddr.contains("@")) {
            throw new RuntimeException("mailgolem: 获取到无效的邮箱地址");
        }

        // token 存储 JSON {csrf, cookies}
        String token = Json.serialize(Map.of("csrf", csrf, "cookies", cookies));
        return new EmailInfo("mailgolem", emailAddr, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 编码的 {csrf, cookies}
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) throw new RuntimeException("mailgolem: token 为空");
        if (email == null || email.isBlank()) throw new RuntimeException("mailgolem: 邮箱地址为空");
        String addr = email.trim();

        // 重新获取 session（原 session 可能已过期）
        HttpResult r1 = HttpClient.get(BASE + "/", defaultHeaders());
        r1.ensureSuccess();
        String cookies = cookieStr(r1);

        Matcher m = CSRF_RE.matcher(r1.getBody());
        if (!m.find()) throw new RuntimeException("mailgolem: 无法提取 CSRF token");
        String newCsrf = m.group(1);

        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json, text/plain, */*");
        h.put("Content-Type", "application/x-www-form-urlencoded");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Referer", BASE + "/");
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Cookie", cookies);

        HttpResult r2 = HttpClient.post(BASE + "/fetch-emails/" + addr,
                "_token=" + ProviderUtil.urlEncode(newCsrf),
                "application/x-www-form-urlencoded", h);
        r2.ensureSuccess();

        JsonElement parsed = Json.parse(r2.getBody());
        if (parsed == null || !parsed.isJsonArray()) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (!item.isJsonObject()) continue;
            JsonObject msg = item.getAsJsonObject();
            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", Json.str(msg, "id"));
            row.put("from", Json.str(msg, "from"));
            row.put("to", addr);
            row.put("subject", Json.str(msg, "subject"));
            out.add(Normalizer.normalizeEmail(row, addr));
        }
        return out;
    }
}
