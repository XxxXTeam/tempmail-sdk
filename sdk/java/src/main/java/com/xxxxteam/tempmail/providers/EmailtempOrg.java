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
 * emailtemp.org 渠道 — https://emailtemp.org
 *
 * <p>Laravel + CSRF，GET /en 获取 session cookie + CSRF token，
 * POST /messages 获取邮箱地址和邮件列表，GET /view/{id} 获取正文。</p>
 */
public final class EmailtempOrg {

    private static final String BASE_URL = "https://emailtemp.org";
    private static final String TOK_PREFIX = "eto1:";
    private static final Pattern CSRF_RE = Pattern.compile(
            "<meta\\s+name=\"csrf-token\"\\s+content=\"([^\"]+)\"");

    private EmailtempOrg() {
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

    private static String encodeToken(String csrf, String cookie) {
        String raw = Json.serialize(Map.of("t", csrf, "c", cookie));
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.getBytes(StandardCharsets.UTF_8));
    }

    private static String[] decodeToken(String token) {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw new RuntimeException("emailtemp-org: 无效的 token");
        }
        String raw = new String(Base64.getDecoder().decode(token.substring(TOK_PREFIX.length())), StandardCharsets.UTF_8);
        JsonObject obj = Json.parseObject(raw);
        if (obj == null) throw new RuntimeException("emailtemp-org: 无效的 token");
        return new String[]{Json.str(obj, "t"), Json.str(obj, "c")};
    }

    private static JsonObject postMessages(String csrf, String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Referer", BASE_URL + "/en");
        h.put("Content-Type", "application/x-www-form-urlencoded");
        h.put("X-CSRF-TOKEN", csrf);
        if (cookie != null && !cookie.isEmpty()) h.put("Cookie", cookie);

        HttpResult resp = HttpClient.post(BASE_URL + "/messages",
                "_token=" + ProviderUtil.urlEncode(csrf) + "&captcha=",
                "application/x-www-form-urlencoded", h);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) throw new RuntimeException("emailtemp-org: 响应 JSON 解析失败");
        return data;
    }

    /**
     * 创建 emailtemp.org 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> bh = new LinkedHashMap<>();
        bh.put("User-Agent", "Mozilla/5.0");
        bh.put("Accept", "text/html");
        HttpResult r1 = HttpClient.get(BASE_URL + "/en", bh);
        r1.ensureSuccess();
        String cookie = cookieFromResp(r1);

        Matcher m = CSRF_RE.matcher(r1.getBody());
        if (!m.find()) throw new RuntimeException("emailtemp-org: 未能提取 CSRF token");
        String csrf = m.group(1);

        JsonObject data = postMessages(csrf, cookie);
        String mailbox = Json.str(data, "mailbox").trim();
        if (mailbox.isEmpty() || !mailbox.contains("@")) {
            throw new RuntimeException("emailtemp-org: 邮箱地址无效");
        }

        return new EmailInfo("emailtemp-org", mailbox, encodeToken(csrf, cookie), null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 编码的 token
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (email == null || email.isBlank()) throw new RuntimeException("emailtemp-org: 邮箱地址为空");
        String[] parts = decodeToken(token);
        String csrf = parts[0];
        String cookie = parts[1];

        JsonObject data = postMessages(csrf, cookie);
        var msgs = Json.arr(data, "messages");
        if (msgs == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : msgs) {
            if (!item.isJsonObject()) continue;
            JsonObject msg = item.getAsJsonObject();
            String mid = Json.str(msg, "id").trim();
            if (mid.isEmpty()) continue;

            String fromEmail = Json.str(msg, "from_email");
            String fromName = Json.str(msg, "from");
            String from = (!fromName.isEmpty() && !fromName.equals(fromEmail))
                    ? fromName + " <" + fromEmail + ">" : fromEmail;

            // 获取 HTML 正文
            String htmlBody = fetchView(cookie, mid);

            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", mid);
            row.put("from", from);
            row.put("to", email);
            row.put("subject", Json.str(msg, "subject"));
            row.put("html", htmlBody);
            out.add(Normalizer.normalizeEmail(row, email));
        }
        return out;
    }

    private static String fetchView(String cookie, String mid) {
        if (mid.isEmpty()) return "";
        try {
            Map<String, String> h = new LinkedHashMap<>();
            h.put("User-Agent", "Mozilla/5.0");
            h.put("Referer", BASE_URL + "/en");
            if (cookie != null && !cookie.isEmpty()) h.put("Cookie", cookie);
            HttpResult resp = HttpClient.get(BASE_URL + "/view/" + ProviderUtil.urlEncode(mid), h);
            return resp.isOk() ? resp.getBody() : "";
        } catch (RuntimeException e) {
            return "";
        }
    }
}
