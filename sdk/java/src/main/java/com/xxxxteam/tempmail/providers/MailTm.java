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

import java.net.URI;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;

/**
 * mail.tm 协议家族实现（mail.tm / duckmail 共用同一套 Hydra REST API，仅 baseUrl 不同）。
 *
 * <p>流程：获取域名 → 随机邮箱/密码 → 创建账号 → 换取 Bearer Token。</p>
 */
public final class MailTm {

    private MailTm() {
    }

    private static Map<String, String> baseHeaders(String baseUrl) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json");
        h.put("Origin", originOf(baseUrl));
        return h;
    }

    private static String originOf(String baseUrl) {
        try {
            URI u = URI.create(baseUrl);
            return u.getScheme() + "://" + u.getHost();
        } catch (RuntimeException e) {
            return baseUrl;
        }
    }

    private static List<String> getDomains(String baseUrl) {
        HttpResult resp = HttpClient.get(baseUrl + "/domains", baseHeaders(baseUrl));
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonArray members = null;
        if (data != null && data.isJsonArray()) {
            members = data.getAsJsonArray();
        } else {
            members = Json.arr(data, "hydra:member");
        }
        List<String> domains = new ArrayList<>();
        if (members != null) {
            for (JsonElement m : members) {
                if (m.isJsonObject()) {
                    JsonObject o = m.getAsJsonObject();
                    boolean active = o.has("isActive") && o.get("isActive").isJsonPrimitive()
                            && o.get("isActive").getAsBoolean();
                    if (active && o.has("domain")) {
                        domains.add(o.get("domain").getAsString());
                    }
                }
            }
        }
        return domains;
    }

    /**
     * 创建临时邮箱。
     *
     * @param channel 渠道标识
     * @param baseUrl API 基地址
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel, String baseUrl) {
        List<String> domains = getDomains(baseUrl);
        if (domains.isEmpty()) {
            throw new RuntimeException("No available domains");
        }
        String address = ProviderUtil.randomString(12) + "@"
                + domains.get(ThreadLocalRandom.current().nextInt(domains.size()));
        String password = ProviderUtil.randomString(16);
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("address", address);
        body.put("password", password);
        String payload = Json.serialize(body);

        Map<String, String> headers = baseHeaders(baseUrl);
        HttpResult accResp = HttpClient.post(baseUrl + "/accounts", payload, "application/ld+json", headers);
        accResp.ensureSuccess();
        JsonElement account = Json.parse(accResp.getBody());

        HttpResult tokResp = HttpClient.post(baseUrl + "/token", payload, "application/json", headers);
        tokResp.ensureSuccess();
        String token = Json.str(Json.parse(tokResp.getBody()), "token");

        return new EmailInfo(channel, address, token, null, Json.str(account, "createdAt"));
    }

    /**
     * 获取邮件列表，逐封拉取详情。
     *
     * @param baseUrl API 基地址
     * @param token   Bearer 令牌
     * @param email   邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String baseUrl, String token, String email) {
        Map<String, String> headers = baseHeaders(baseUrl);
        headers.put("Authorization", "Bearer " + (token != null ? token : ""));

        HttpResult resp = HttpClient.get(baseUrl + "/messages", headers);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonArray messages;
        if (data != null && data.isJsonArray()) {
            messages = data.getAsJsonArray();
        } else {
            messages = Json.arr(data, "hydra:member");
        }

        List<Email> result = new ArrayList<>();
        if (messages == null) {
            return result;
        }
        for (JsonElement msg : messages) {
            if (!msg.isJsonObject()) {
                continue;
            }
            JsonObject mo = msg.getAsJsonObject();
            String id = Json.str(mo, "id");
            JsonObject detail = mo;
            if (!id.isEmpty()) {
                try {
                    HttpResult dr = HttpClient.get(baseUrl + "/messages/" + id, headers);
                    if (dr.isOk()) {
                        JsonObject det = Json.parseObject(dr.getBody());
                        if (det != null) {
                            detail = det;
                        }
                    }
                } catch (RuntimeException ignored) {
                    // 回退到列表摘要
                }
            }
            result.add(Normalizer.normalizeEmail(flatten(detail, email), email));
        }
        return result;
    }

    private static Map<String, Object> flatten(JsonObject msg, String recipient) {
        String from = "";
        if (msg.has("from") && msg.get("from").isJsonObject()) {
            JsonObject fo = msg.getAsJsonObject("from");
            from = fo.has("address") ? fo.get("address").getAsString() : "";
        } else if (msg.has("from") && msg.get("from").isJsonPrimitive()) {
            from = msg.get("from").getAsString();
        }

        String to = recipient;
        if (msg.has("to") && msg.get("to").isJsonArray()) {
            JsonArray ta = msg.getAsJsonArray("to");
            if (ta.size() > 0 && ta.get(0).isJsonObject()) {
                JsonObject t0 = ta.get(0).getAsJsonObject();
                to = t0.has("address") ? t0.get("address").getAsString() : recipient;
            }
        }

        String html = "";
        if (msg.has("html") && msg.get("html").isJsonArray()) {
            for (JsonElement h : msg.getAsJsonArray("html")) {
                if (h.isJsonPrimitive()) {
                    html += h.getAsString();
                }
            }
        } else {
            html = Json.str(msg, "html");
        }

        Map<String, Object> dict = new LinkedHashMap<>();
        dict.put("id", Json.str(msg, "id"));
        dict.put("from", from);
        dict.put("to", to);
        dict.put("subject", Json.str(msg, "subject"));
        dict.put("text", Json.str(msg, "text"));
        dict.put("html", html);
        dict.put("createdAt", Json.str(msg, "createdAt"));
        dict.put("seen", msg.has("seen") && msg.get("seen").isJsonPrimitive() && msg.get("seen").getAsBoolean());
        return dict;
    }
}
