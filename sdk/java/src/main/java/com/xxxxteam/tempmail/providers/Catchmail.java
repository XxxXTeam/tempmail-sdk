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
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Catchmail 渠道（catchmail.io）。
 *
 * <p>域名别名：catchmail.io / mailistry.com / zeppost.com</p>
 */
public final class Catchmail {

    private static final String API_BASE = "https://api.catchmail.io/api/v1";
    private static final String DEFAULT_DOMAIN = "catchmail.io";
    private static final Pattern ANGLE_ADDR = Pattern.compile("<([^>]+)>");

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("Referer", "https://catchmail.io/");
        HEADERS.put("Origin", "https://catchmail.io");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    }

    private Catchmail() {
    }

    /**
     * 创建临时邮箱。
     *
     * @param channel 渠道标识
     * @param domain  指定域名，可为 null（默认 catchmail.io）
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel, String domain) {
        String selectedDomain = pickDomain(domain);
        String email = ProviderUtil.randomLocalSdk() + "@" + selectedDomain;

        // 调用 mailbox 接口验证地址可用
        HttpResult resp = HttpClient.get(
                API_BASE + "/mailbox?address=" + ProviderUtil.urlEncode(email), HEADERS);
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
            throw new IllegalArgumentException("catchmail: empty email");
        }
        String address = email.trim();

        HttpResult resp = HttpClient.get(
                API_BASE + "/mailbox?address=" + ProviderUtil.urlEncode(address), HEADERS);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) {
            return new ArrayList<>();
        }
        JsonArray rows = Json.arr(data, "messages");
        if (rows == null) {
            return new ArrayList<>();
        }

        List<Email> result = new ArrayList<>();
        for (JsonElement row : rows) {
            if (!row.isJsonObject()) {
                continue;
            }
            JsonObject item = row.getAsJsonObject();
            String messageId = Json.str(item, "id").trim();
            if (messageId.isEmpty()) {
                continue;
            }
            // 尝试获取详情
            JsonObject detail = fetchMessage(messageId, address);
            if (detail != null) {
                result.add(Normalizer.normalizeEmail(flattenDetail(detail, address), address));
            } else {
                // 回退到列表摘要
                Map<String, Object> flat = new LinkedHashMap<>();
                flat.put("id", messageId);
                flat.put("from", cleanAddress(Json.str(item, "from")));
                flat.put("to", ProviderUtil.firstNonEmpty(Json.str(item, "mailbox"), address));
                flat.put("subject", Json.str(item, "subject"));
                flat.put("date", Json.str(item, "date"));
                result.add(Normalizer.normalizeEmail(flat, address));
            }
        }
        return result;
    }

    private static JsonObject fetchMessage(String messageId, String email) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/message/" + ProviderUtil.urlEncode(messageId)
                            + "?mailbox=" + ProviderUtil.urlEncode(email),
                    HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException ignored) {
            return null;
        }
    }

    private static Map<String, Object> flattenDetail(JsonObject raw, String recipient) {
        JsonObject body = raw.has("body") && raw.get("body").isJsonObject()
                ? raw.getAsJsonObject("body") : null;
        Map<String, Object> flat = new LinkedHashMap<>();
        flat.put("id", Json.str(raw, "id"));
        flat.put("from", cleanAddress(Json.str(raw, "from")));
        String to = cleanAddress(Json.str(raw, "to"));
        flat.put("to", to.isEmpty() ? recipient : to);
        flat.put("subject", Json.str(raw, "subject"));
        flat.put("text", body != null ? Json.str(body, "text") : "");
        flat.put("html", body != null ? Json.str(body, "html") : "");
        flat.put("date", Json.str(raw, "date"));
        return flat;
    }

    private static String cleanAddress(String value) {
        if (value == null || value.isEmpty()) {
            return "";
        }
        Matcher m = ANGLE_ADDR.matcher(value);
        if (m.find()) {
            return m.group(1).trim();
        }
        return value.trim();
    }

    private static String pickDomain(String preferred) {
        String p = (preferred != null ? preferred : "").trim();
        if (p.startsWith("@")) {
            p = p.substring(1);
        }
        p = p.toLowerCase();
        if (!p.isEmpty()) {
            for (String domain : new String[]{"catchmail.io", "mailistry.com", "zeppost.com"}) {
                if (domain.equals(p)) {
                    return domain;
                }
            }
        }
        return DEFAULT_DOMAIN;
    }
}
