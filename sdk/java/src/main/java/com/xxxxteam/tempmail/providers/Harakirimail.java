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

/**
 * HarakiriMail 渠道（harakirimail.com）。
 * 无需认证、无需 Cookie、无需 Token 的简单 REST API。
 */
public final class Harakirimail {

    private static final String BASE = "https://harakirimail.com";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json, text/plain, */*");
        HEADERS.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    }

    private Harakirimail() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String name = ProviderUtil.randomString(12);
        String email = name + "@harakirimail.com";
        // 调用收件箱接口验证地址可用
        HttpResult resp = HttpClient.get(BASE + "/api/v1/inbox/" + name, HEADERS);
        resp.ensureSuccess();
        return new EmailInfo("harakirimail", email);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("harakirimail: 邮箱地址为空");
        }
        String e = email.trim();
        String name = e.contains("@") ? e.split("@", 2)[0] : e;

        HttpResult resp = HttpClient.get(BASE + "/api/v1/inbox/" + ProviderUtil.urlEncode(name), HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonObject()) {
            return result;
        }
        JsonElement rowsEl = parsed.getAsJsonObject().get("emails");
        if (rowsEl == null || !rowsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement item : rowsEl.getAsJsonArray()) {
            if (!item.isJsonObject()) {
                continue;
            }
            Map<String, Object> raw = Json.toDict(item);
            String emailId = raw.get("_id") != null ? raw.get("_id").toString() : "";
            // 获取单封邮件详情以拿到正文
            Map<String, Object> detail = fetchBody(emailId);
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", emailId);
            flat.put("from", raw.getOrDefault("from", ""));
            flat.put("to", e);
            flat.put("subject", raw.getOrDefault("subject", ""));
            flat.put("date", raw.getOrDefault("received", ""));
            flat.put("html", ProviderUtil.firstNonEmpty(
                    str(detail, "body_html"), str(detail, "html")));
            flat.put("text", ProviderUtil.firstNonEmpty(
                    str(detail, "body_text"), str(detail, "text")));
            result.add(Normalizer.normalizeEmail(flat, e));
        }
        return result;
    }

    private static Map<String, Object> fetchBody(String emailId) {
        if (emailId == null || emailId.isEmpty()) {
            return new LinkedHashMap<>();
        }
        try {
            HttpResult resp = HttpClient.get(
                    BASE + "/api/v1/email/" + ProviderUtil.urlEncode(emailId), HEADERS);
            resp.ensureSuccess();
            JsonElement parsed = Json.parse(resp.getBody());
            if (parsed != null && parsed.isJsonObject()) {
                return Json.toDict(parsed);
            }
        } catch (RuntimeException ignored) {
        }
        return new LinkedHashMap<>();
    }

    private static String str(Map<String, Object> m, String key) {
        Object v = m.get(key);
        return v != null ? v.toString() : "";
    }
}
