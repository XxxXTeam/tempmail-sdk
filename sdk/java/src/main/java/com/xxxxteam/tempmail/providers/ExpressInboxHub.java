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
 * ExpressInboxHub 渠道 — https://expressinboxhub.com
 *
 * <p>GET / 获取 CSRF token 和 session cookies → POST /messages 创建邮箱和获取邮件。</p>
 */
public final class ExpressInboxHub {

    private static final String BASE = "https://expressinboxhub.com";
    private static final Pattern CSRF_RE = Pattern.compile(
            "<meta\\s+name=[\"']csrf-token[\"']\\s+content=[\"']([^\"']+)[\"']");

    private ExpressInboxHub() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json, text/plain, */*");
        h.put("Content-Type", "application/json");
        h.put("Origin", BASE);
        h.put("Referer", BASE + "/");
        h.put("User-Agent", "Mozilla/5.0");
        return h;
    }

    private static String[] initSession() {
        Map<String, String> bh = new LinkedHashMap<>();
        bh.put("User-Agent", "Mozilla/5.0");
        bh.put("Accept", "text/html");
        HttpResult resp = HttpClient.get(BASE, bh);
        resp.ensureSuccess();

        Matcher m = CSRF_RE.matcher(resp.getBody());
        if (!m.find()) throw new RuntimeException("expressinboxhub: 无法提取 CSRF token");
        String csrf = m.group(1);

        StringBuilder cookies = new StringBuilder();
        for (String sc : resp.getSetCookies()) {
            String seg = sc.split(";")[0].trim();
            if (cookies.length() > 0) cookies.append("; ");
            cookies.append(seg);
        }
        if (cookies.isEmpty()) throw new RuntimeException("expressinboxhub: 未获取到 session cookies");
        return new String[]{csrf, cookies.toString()};
    }

    /**
     * 创建 expressinboxhub 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String[] session = initSession();
        String csrf = session[0];
        String cookies = session[1];

        Map<String, String> h = headers();
        h.put("X-CSRF-TOKEN", csrf);
        h.put("Cookie", cookies);

        String payload = Json.serialize(Map.of("_token", csrf));
        HttpResult resp = HttpClient.post(BASE + "/messages", payload, "application/json", h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) throw new RuntimeException("expressinboxhub: 响应格式异常");

        String mailbox = Json.str(body, "mailbox").trim();
        if (mailbox.isEmpty()) throw new RuntimeException("expressinboxhub: 响应中缺少 mailbox");

        String token = Json.serialize(Map.of("csrfToken", csrf, "cookies", cookies));
        return new EmailInfo("expressinboxhub", mailbox, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 编码的 {csrfToken, cookies}
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) throw new RuntimeException("expressinboxhub: token 为空");

        JsonObject tok = Json.parseObject(token);
        if (tok == null) throw new RuntimeException("expressinboxhub: token 格式无效");
        String csrf = Json.str(tok, "csrfToken");
        String cookies = Json.str(tok, "cookies");

        Map<String, String> h = headers();
        h.put("X-CSRF-TOKEN", csrf);
        h.put("Cookie", cookies);

        String payload = Json.serialize(Map.of("_token", csrf));
        HttpResult resp = HttpClient.post(BASE + "/messages", payload, "application/json", h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();
        var messages = Json.arr(body, "messages");
        if (messages == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : messages) {
            if (!item.isJsonObject()) continue;
            Map<String, Object> raw = Json.toDict(item);
            // 映射 receivedAt → date, content → html
            if (raw.containsKey("receivedAt") && !raw.containsKey("date")) {
                raw.put("date", raw.get("receivedAt"));
            }
            if (raw.containsKey("content") && !raw.containsKey("html")) {
                raw.put("html", raw.get("content"));
            }
            out.add(Normalizer.normalizeEmail(raw, email));
        }
        return out;
    }
}
