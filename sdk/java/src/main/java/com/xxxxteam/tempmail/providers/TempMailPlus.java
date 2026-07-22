package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
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
 * TempMail Plus 渠道（tempmail.plus）。
 *
 * <p>无需认证、无需 Cookie、无需 Token 的简单 REST API 渠道。
 * 域名别名：mailto.plus / fexpost.com / fexbox.org / mailbox.in.ua / rover.info /
 * chitthi.in / fextemp.com / any.pink / merepost.com</p>
 */
public final class TempMailPlus {

    private static final String API_BASE = "https://tempmail.plus/api/mails";
    private static final String DEFAULT_DOMAIN = "mailto.plus";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36");
        HEADERS.put("Referer", "https://tempmail.plus/");
        HEADERS.put("Origin", "https://tempmail.plus");
    }

    private TempMailPlus() {
    }

    /**
     * 创建临时邮箱。
     *
     * @param channel 渠道标识
     * @param domain  指定域名，可为 null（默认 mailto.plus）
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel, String domain) {
        String selectedDomain = (domain != null && !domain.isBlank()) ? domain.trim() : DEFAULT_DOMAIN;
        String local = ProviderUtil.randomString(12);
        String email = local + "@" + selectedDomain;

        // 调用列表接口验证地址可用
        HttpResult resp = HttpClient.get(API_BASE + "/?email=" + ProviderUtil.urlEncode(email) + "&epin=", HEADERS);
        resp.ensureSuccess();

        return new EmailInfo(channel, email);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("tempmail-plus: 邮箱地址为空");
        }
        String e = email.trim();

        HttpResult resp = HttpClient.get(API_BASE + "/?email=" + ProviderUtil.urlEncode(e) + "&epin=", HEADERS);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) {
            return new ArrayList<>();
        }
        JsonArray rows = Json.arr(data, "mail_list");
        if (rows == null) {
            return new ArrayList<>();
        }

        List<Email> result = new ArrayList<>();
        for (JsonElement row : rows) {
            if (!row.isJsonObject()) {
                continue;
            }
            JsonObject raw = row.getAsJsonObject();
            String mailId = Json.str(raw, "mail_id");

            // 获取单封邮件详情
            Map<String, Object> detail = fetchBody(mailId, e);

            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", mailId.isEmpty() ? "" : mailId);
            flat.put("from", ProviderUtil.firstNonEmpty(Json.str(raw, "from_mail"), Json.str(raw, "from_name")));
            flat.put("to", e);
            flat.put("subject", Json.str(raw, "subject"));
            flat.put("date", Json.str(raw, "time"));
            flat.put("html", detail.getOrDefault("html", ""));
            flat.put("text", detail.getOrDefault("text", ""));
            boolean isNew = raw.has("is_new") && raw.get("is_new").isJsonPrimitive()
                    && raw.get("is_new").getAsBoolean();
            flat.put("isRead", !isNew);
            result.add(Normalizer.normalizeEmail(flat, e));
        }
        return result;
    }

    /**
     * 获取单封邮件正文。
     */
    private static Map<String, Object> fetchBody(String mailId, String email) {
        Map<String, Object> empty = new LinkedHashMap<>();
        if (mailId == null || mailId.isEmpty()) {
            return empty;
        }
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/" + ProviderUtil.urlEncode(mailId) + "?email=" + ProviderUtil.urlEncode(email) + "&epin=",
                    HEADERS);
            if (!resp.isOk()) {
                return empty;
            }
            JsonObject obj = Json.parseObject(resp.getBody());
            if (obj == null) {
                return empty;
            }
            Map<String, Object> result = new LinkedHashMap<>();
            result.put("html", Json.str(obj, "html"));
            result.put("text", Json.str(obj, "text"));
            return result;
        } catch (RuntimeException ignored) {
            return empty;
        }
    }
}
