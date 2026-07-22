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
 * Mailmomy 及其域名变体渠道（mailmomy.com）。
 *
 * <p>免注册、无鉴权的纯 GET JSON API，Token 即邮箱地址本身。</p>
 */
public final class Mailmomy {

    private static final String BASE_URL = "https://mailmomy.com";
    private static final String DEFAULT_DOMAIN = "mailmomy.com";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
    }

    private Mailmomy() {
    }

    private static String pickDomain() {
        try {
            HttpResult resp = HttpClient.get(BASE_URL + "/api/domains/active", HEADERS);
            resp.ensureSuccess();
            JsonElement el = Json.parse(resp.getBody());
            if (el != null && el.isJsonArray()) {
                List<String> domains = new ArrayList<>();
                for (JsonElement d : el.getAsJsonArray()) {
                    if (d.isJsonPrimitive() && d.getAsJsonPrimitive().isString()) {
                        String s = d.getAsString();
                        if (!s.isEmpty()) {
                            domains.add(s);
                        }
                    }
                }
                if (!domains.isEmpty()) {
                    return domains.get(java.util.concurrent.ThreadLocalRandom.current().nextInt(domains.size()));
                }
            }
        } catch (RuntimeException ignored) {
            // 回退默认域名
        }
        return DEFAULT_DOMAIN;
    }

    /**
     * 创建邮箱。domain 为空则从活跃域名池随机取；channel 为对外标识。
     *
     * @param channel 渠道标识
     * @param domain  指定域名，可为 null
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel, String domain) {
        String d = (domain == null || domain.isEmpty()) ? pickDomain() : domain;
        String email = ProviderUtil.randomString(10) + "@" + d;
        return new EmailInfo(channel, email, email, null, null);
    }

    /**
     * 使用默认渠道创建邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        return generate("mailmomy", null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        String addr = email != null ? email.trim() : "";
        if (addr.isEmpty()) {
            throw new RuntimeException("mailmomy: 缺少邮箱地址");
        }
        HttpResult resp = HttpClient.get(
                BASE_URL + "/api/mail/messages?to=" + ProviderUtil.urlEncode(addr) + "&page=1&limit=20", HEADERS);
        resp.ensureSuccess();
        JsonArray emails = Json.arr(Json.parse(resp.getBody()), "emails");
        List<Email> result = new ArrayList<>();
        if (emails == null) {
            return result;
        }
        for (JsonElement msg : emails) {
            if (!msg.isJsonObject()) {
                continue;
            }
            JsonObject mo = msg.getAsJsonObject();
            String message = Json.str(mo, "message");
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(mo, "id"));
            raw.put("from", Json.str(mo, "from"));
            raw.put("to", Json.str(mo, "recipient").isEmpty() ? addr : Json.str(mo, "recipient"));
            raw.put("subject", Json.str(mo, "subject"));
            raw.put("text", Json.str(mo, "bodyText").isEmpty() ? message : Json.str(mo, "bodyText"));
            raw.put("html", message);
            raw.put("date", Json.str(mo, "receivedAt"));
            result.add(Normalizer.normalizeEmail(raw, addr));
        }
        return result;
    }
}
