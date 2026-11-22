package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * tempmails.cc 渠道（tempmails.cc）。
 *
 * <p>地址即邮箱：在公开免费域名上生成地址，无需注册。
 * 详情正文位于 textBody / htmlBody。</p>
 */
public final class TempmailsCc {

    private static final String API_BASE = "https://tempmails.cc";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
    }

    private TempmailsCc() {
    }

    private static String pickDomain() {
        HttpResult resp = HttpClient.get(API_BASE + "/api/domains", HEADERS);
        resp.ensureSuccess();
        JsonArray items = Json.arr(Json.parseObject(resp.getBody()), "domainInfo");
        if (items != null) {
            for (JsonElement item : items) {
                if (item.isJsonObject()) {
                    String domain = Json.str(item.getAsJsonObject(), "domain").trim();
                    if (!domain.isEmpty()) {
                        return domain;
                    }
                }
            }
        }
        throw new RuntimeException("tempmails-cc: no public domain");
    }

    /**
     * 在公开免费域名上生成地址。
     *
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel) {
        String domain = pickDomain();
        return new EmailInfo(channel, ProviderUtil.randomString(16) + "@" + domain);
    }

    /**
     * 跳过选域，在指定固定域名上生成地址。
     *
     * @param channel 渠道标识
     * @param domain  固定域名
     * @return 邮箱信息
     */
    public static EmailInfo generateAtDomain(String channel, String domain) {
        if (domain == null || domain.isBlank()) {
            throw new RuntimeException("tempmails-cc: empty domain");
        }
        return new EmailInfo(channel, ProviderUtil.randomString(16) + "@" + domain.trim());
    }

    /**
     * 获取 tempmails.cc 邮件，详情正文位于 textBody / htmlBody。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("tempmails-cc: empty email");
        }
        String address = email.trim();
        HttpResult resp = HttpClient.get(
                API_BASE + "/api/inbox/" + ProviderUtil.urlEncode(address), HEADERS);
        resp.ensureSuccess();
        JsonArray rows = Json.arr(Json.parseObject(resp.getBody()), "messages");
        List<Email> result = new ArrayList<>();
        if (rows == null) {
            return result;
        }
        for (JsonElement row : rows) {
            if (!row.isJsonObject()) {
                continue;
            }
            JsonObject item = row.getAsJsonObject();
            String messageId = Json.str(item, "id").trim();
            if (messageId.isEmpty()) {
                continue;
            }
            JsonObject detail = fetchDetail(messageId, address);
            if (detail != null) {
                Map<String, Object> flat = new LinkedHashMap<>();
                flat.put("id", ProviderUtil.firstNonEmpty(Json.str(detail, "id"), messageId));
                flat.put("from", Json.str(detail, "from"));
                flat.put("to", address);
                flat.put("subject", Json.str(detail, "subject"));
                flat.put("text", Json.str(detail, "textBody"));
                flat.put("html", Json.str(detail, "htmlBody"));
                flat.put("date", Json.str(detail, "date"));
                flat.put("seen", Json.str(detail, "isRead"));
                result.add(Normalizer.normalizeEmail(flat, address));
                continue;
            }
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", messageId);
            flat.put("from", Json.str(item, "from"));
            flat.put("to", address);
            flat.put("subject", Json.str(item, "subject"));
            flat.put("date", Json.str(item, "date"));
            flat.put("seen", Json.str(item, "isRead"));
            result.add(Normalizer.normalizeEmail(flat, address));
        }
        return result;
    }

    private static JsonObject fetchDetail(String messageId, String address) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/api/message/" + ProviderUtil.urlEncode(messageId)
                            + "?address=" + ProviderUtil.urlEncode(address)
                            + "&mark_read=false",
                    HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException ignored) {
            return null;
        }
    }
}
