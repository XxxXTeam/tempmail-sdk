package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
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
 * MailholeDe 渠道实现 — https://mailhole.de
 *
 * <p>公共临时邮箱，无需认证。Token 即邮箱地址本身。</p>
 */
public final class MailholeDe {

    private static final String BASE_URL = "https://mailhole.de";
    private static final Pattern EMAIL_RE = Pattern.compile("([a-z0-9.]+@mailhole\\.de)");

    private MailholeDe() {
    }

    /**
     * 创建 mailhole.de 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("Accept", "text/html");
        HttpResult resp = HttpClient.get(BASE_URL + "/api/random", headers);
        resp.ensureSuccess();
        Matcher m = EMAIL_RE.matcher(resp.getBody());
        if (!m.find()) {
            throw new RuntimeException("mailhole-de: 无法从响应中解析邮箱地址");
        }
        String email = m.group(1);
        return new EmailInfo("mailhole-de", email, email, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址（token 或 email 均可）
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        String addr = (email != null ? email : "").trim();
        if (addr.isEmpty()) {
            throw new RuntimeException("mailhole-de: 缺少邮箱地址");
        }
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("Accept", "application/json");
        HttpResult resp = HttpClient.get(
                BASE_URL + "/json/" + ProviderUtil.urlEncode(addr), headers);
        resp.ensureSuccess();
        String body = resp.getBody();
        if (body == null || body.isEmpty() || "[]".equals(body.trim())) {
            return new ArrayList<>();
        }
        JsonElement data = Json.parse(body);
        List<Email> result = new ArrayList<>();
        if (data == null || !data.isJsonArray()) {
            return result;
        }
        for (JsonElement item : data.getAsJsonArray()) {
            if (!item.isJsonObject()) {
                continue;
            }
            var msg = item.getAsJsonObject();
            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", Json.str(msg, "id"));
            row.put("from", firstStr(msg, "sender", "from"));
            row.put("to", addr);
            row.put("subject", Json.str(msg, "subject"));
            row.put("text", firstStr(msg, "body", "text"));
            row.put("html", firstStr(msg, "html", "body"));
            row.put("received_at", firstStr(msg, "date", "received"));
            result.add(Normalizer.normalizeEmail(row, addr));
        }
        return result;
    }

    private static String firstStr(com.google.gson.JsonObject msg, String... keys) {
        for (String key : keys) {
            String val = Json.str(msg, key);
            if (!val.isEmpty()) {
                return val;
            }
        }
        return "";
    }
}
