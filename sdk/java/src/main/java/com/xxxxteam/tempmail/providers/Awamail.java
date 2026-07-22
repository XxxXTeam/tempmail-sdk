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

/**
 * awamail.com 渠道 — https://awamail.com
 *
 * <p>POST /welcome/change_mailbox 创建邮箱，提取 awamail_session cookie。
 * GET /welcome/get_emails 获取邮件列表。</p>
 */
public final class Awamail {

    private static final String BASE_URL = "https://awamail.com/welcome";

    private Awamail() {
    }

    private static Map<String, String> defaultHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("Origin", "https://awamail.com");
        h.put("Referer", "https://awamail.com/?lang=zh");
        return h;
    }

    /**
     * 创建临时邮箱，提取 awamail_session cookie 作为 token。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> h = defaultHeaders();
        h.put("Content-Length", "0");
        HttpResult resp = HttpClient.post(BASE_URL + "/change_mailbox", "", "application/json", h);
        resp.ensureSuccess();

        String sessionCookie = "";
        for (String c : resp.getSetCookies()) {
            int idx = c.indexOf("awamail_session=");
            if (idx >= 0) {
                String seg = c.substring(idx);
                int semi = seg.indexOf(';');
                sessionCookie = semi < 0 ? seg : seg.substring(0, semi);
                break;
            }
        }
        if (sessionCookie.isEmpty()) {
            throw new RuntimeException("awamail: 未获取到 session cookie");
        }

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null || !body.has("success") || !body.get("success").getAsBoolean()) {
            throw new RuntimeException("awamail: 创建邮箱失败");
        }
        JsonObject data = body.has("data") && body.get("data").isJsonObject()
                ? body.getAsJsonObject("data") : null;
        if (data == null) throw new RuntimeException("awamail: 响应缺少 data");

        String email = Json.str(data, "email_address");
        return new EmailInfo("awamail", email, sessionCookie, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token 会话 cookie
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        Map<String, String> h = defaultHeaders();
        h.put("Cookie", token != null ? token : "");
        h.put("X-Requested-With", "XMLHttpRequest");

        HttpResult resp = HttpClient.get(BASE_URL + "/get_emails", h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null || !body.has("success") || !body.get("success").getAsBoolean()) {
            throw new RuntimeException("awamail: 获取邮件失败");
        }
        JsonObject data = body.has("data") && body.get("data").isJsonObject()
                ? body.getAsJsonObject("data") : null;
        if (data == null) return new ArrayList<>();

        var emails = Json.arr(data, "emails");
        if (emails == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : emails) {
            if (item.isJsonObject()) {
                out.add(Normalizer.normalizeEmail(Json.toDict(item), email));
            }
        }
        return out;
    }
}
