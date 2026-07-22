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
 * linshiyouxiang.net 渠道 — https://www.linshiyouxiang.net
 *
 * <p>GET / 从 HTML 提取 tempMailGlobal（邮箱）和 mailCodeGlobal（校验 code）。
 * POST /get-messages 获取邮件。</p>
 */
public final class LinshiyouxiangNet {

    private static final String BASE_URL = "https://www.linshiyouxiang.net";
    private static final java.util.regex.Pattern EMAIL_RE =
            java.util.regex.Pattern.compile("tempMailGlobal\\s*=\\s*'([^']+)'");
    private static final java.util.regex.Pattern CODE_RE =
            java.util.regex.Pattern.compile("mailCodeGlobal\\s*=\\s*'([^']+)'");

    private LinshiyouxiangNet() {
    }

    /**
     * 创建 linshiyouxiang.net 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "text/html");
        HttpResult resp = HttpClient.get(BASE_URL + "/", h);
        resp.ensureSuccess();

        String html = resp.getBody();
        java.util.regex.Matcher em = EMAIL_RE.matcher(html);
        if (!em.find()) throw new RuntimeException("linshiyouxiang-net: 未能提取邮箱地址");
        String email = em.group(1).trim();

        java.util.regex.Matcher cm = CODE_RE.matcher(html);
        String code = cm.find() ? cm.group(1).trim() : "";

        return new EmailInfo("linshiyouxiang-net", email, code, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token mailCodeGlobal 值
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("linshiyouxiang-net: 邮箱地址为空");
        }

        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json");
        h.put("Content-Type", "application/json");
        h.put("Referer", BASE_URL + "/");
        h.put("Origin", BASE_URL);
        h.put("User-Agent", "Mozilla/5.0");
        h.put("X-Requested-With", "XMLHttpRequest");

        String payload = Json.serialize(Map.of("email", email, "code", token != null ? token : ""));
        HttpResult resp = HttpClient.post(BASE_URL + "/get-messages", payload, "application/json", h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();
        var emails = Json.arr(body, "emails");
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
