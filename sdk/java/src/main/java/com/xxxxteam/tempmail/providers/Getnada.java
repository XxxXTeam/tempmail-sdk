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
 * GetNada 渠道及其众多域名别名（getnada.net/api）。
 */
public final class Getnada {

    private static final String API_BASE = "https://getnada.net/api";
    private static final Map<String, String> HEADERS_JSON = new LinkedHashMap<>();

    static {
        HEADERS_JSON.put("Accept", "application/json");
    }

    private Getnada() {
    }

    private static String pickDomain(String preferred) {
        HttpResult resp = HttpClient.get(API_BASE + "/public/domains", HEADERS_JSON);
        resp.ensureSuccess();
        JsonArray arr = Json.arr(Json.parse(resp.getBody()), "domains");
        List<String> domains = new ArrayList<>();
        if (arr != null) {
            for (JsonElement d : arr) {
                if (d != null && d.isJsonPrimitive()) {
                    String s = d.getAsString().trim().toLowerCase();
                    if (!s.isEmpty() && s.length() <= 253 && s.contains(".")) {
                        domains.add(s);
                    }
                }
            }
        }
        String want = (preferred != null ? preferred : "").trim();
        if (want.startsWith("@")) {
            want = want.substring(1);
        }
        want = want.toLowerCase();
        if (!want.isEmpty()) {
            for (String d : domains) {
                if (d.equals(want)) {
                    return d;
                }
            }
            throw new RuntimeException("getnada: domain not available: " + want);
        }
        for (String d : domains) {
            if (d.equals("getnada.net")) {
                return d;
            }
        }
        if (!domains.isEmpty()) {
            return domains.get(0);
        }
        throw new RuntimeException("getnada: no domain available");
    }

    /**
     * 创建邮箱。domain 指定域名别名，channel 为对外渠道标识。
     *
     * @param domain  指定域名，可为 null
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain, String channel) {
        String selected = pickDomain(domain);
        String requested = ProviderUtil.randomLocalSdk() + "@" + selected;
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("email", requested);
        String payload = Json.serialize(body);
        HttpResult resp = HttpClient.post(API_BASE + "/inbox/open", payload, "application/json", HEADERS_JSON);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) {
            throw new RuntimeException("getnada: invalid open response");
        }
        String token = Json.str(data, "token");
        String email = Json.str(data, "recipient");
        if (email.isEmpty()) {
            email = requested;
        }
        if (token.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("getnada: invalid open response");
        }
        Long expiresAt = null;
        if (data.has("activeUntil") && data.get("activeUntil").isJsonPrimitive()) {
            try {
                expiresAt = data.get("activeUntil").getAsLong();
            } catch (RuntimeException ignored) {
                // 忽略无法解析的过期时间
            }
        }
        return new EmailInfo(channel, email, token, expiresAt, null);
    }

    /**
     * 获取邮件列表，逐封拉取详情。
     *
     * @param token 会话令牌
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String auth = (token != null ? token : "").trim();
        if (auth.isEmpty()) {
            throw new RuntimeException("getnada: empty token");
        }
        HttpResult resp = HttpClient.get(
                API_BASE + "/inbox/messages?token=" + ProviderUtil.urlEncode(auth), HEADERS_JSON);
        resp.ensureSuccess();
        JsonArray rows = Json.arr(Json.parse(resp.getBody()), "messages");
        List<Email> result = new ArrayList<>();
        if (rows == null) {
            return result;
        }
        for (JsonElement item : rows) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject row = item.getAsJsonObject();
            String id = Json.str(row, "id");
            JsonObject src = row;
            if (!id.isEmpty()) {
                try {
                    HttpResult dr = HttpClient.get(API_BASE + "/inbox/message?id=" + ProviderUtil.urlEncode(id)
                            + "&token=" + ProviderUtil.urlEncode(auth), HEADERS_JSON);
                    if (dr.isOk()) {
                        JsonObject det = Json.parseObject(dr.getBody());
                        if (det != null && det.has("message") && det.get("message").isJsonObject()) {
                            src = det.getAsJsonObject("message");
                        }
                    }
                } catch (RuntimeException ignored) {
                    // 回退列表摘要
                }
            }
            result.add(Normalizer.normalizeEmail(flatten(src, email), email));
        }
        return result;
    }

    private static Map<String, Object> flatten(JsonObject raw, String recipient) {
        Map<String, Object> dict = Json.toDict(raw);
        dict.put("from", ProviderUtil.firstNonEmpty(Json.str(raw, "from_addr"), Json.str(raw, "from")));
        String to = Json.str(raw, "to");
        dict.put("to", to.isEmpty() ? recipient : to);
        dict.put("text", ProviderUtil.firstNonEmpty(Json.str(raw, "text_plain"), Json.str(raw, "text")));
        dict.put("html", ProviderUtil.firstNonEmpty(Json.str(raw, "html_sanitized"), Json.str(raw, "html")));
        if (raw.has("read_at")) {
            dict.put("read", !Json.str(raw, "read_at").isEmpty());
        }
        return dict;
    }
}
