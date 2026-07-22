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
 * Fmail 渠道（fmail.men）。
 */
public final class Fmail {

    private static final String BASE_URL = "https://fmail.men";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36");
    }

    private Fmail() {
    }

    /**
     * 将原始消息字段扁平化为标准字典。
     */
    private static Map<String, Object> flattenMessage(Map<String, Object> raw, String recipient) {
        Map<String, Object> out = new LinkedHashMap<>();
        out.put("id", firstStr(raw, "id", "uid", "token"));
        out.put("from", firstStr(raw, "sender", "from"));
        out.put("to", firstStr(raw, "recipient", "to").isEmpty() ? recipient : firstStr(raw, "recipient", "to"));
        out.put("subject", firstStr(raw, "subject"));
        out.put("text", firstStr(raw, "body_text", "text", "snippet"));
        out.put("html", firstStr(raw, "body_html", "html"));
        out.put("date", firstStr(raw, "received_at", "timestamp", "date"));
        return out;
    }

    private static String firstStr(Map<String, Object> m, String... keys) {
        for (String k : keys) {
            Object v = m.get(k);
            if (v != null && !v.toString().isEmpty()) {
                return v.toString();
            }
        }
        return "";
    }

    /**
     * 创建临时邮箱（支持指定域名）。
     *
     * @param domain 域名，可为 null
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        String path = "/v1/random";
        if (domain != null && !domain.isBlank()) {
            String d = domain.trim().startsWith("@") ? domain.trim().substring(1) : domain.trim();
            path += "?domain=" + ProviderUtil.urlEncode(d);
        }
        HttpResult resp = HttpClient.get(BASE_URL + path, HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("fmail: invalid random response");
        }
        String email = Json.str(parsed, "address").trim();
        if (email.isEmpty()) {
            String username = Json.str(parsed, "username").trim();
            String dom = Json.str(parsed, "domain").trim();
            if (!username.isEmpty() && !dom.isEmpty()) {
                email = username + "@" + dom;
            }
        }
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("fmail: invalid random response");
        }
        return new EmailInfo("fmail", email);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("fmail: invalid email");
        }
        String value = email.trim();
        int atIdx = value.indexOf('@');
        if (atIdx < 0) {
            throw new RuntimeException("fmail: invalid email");
        }
        String local = value.substring(0, atIdx);
        String domain = value.substring(atIdx + 1);

        HttpResult resp = HttpClient.get(
                BASE_URL + "/v1/inbox/" + ProviderUtil.urlEncode(local)
                        + "?domain=" + ProviderUtil.urlEncode(domain) + "&limit=50", HEADERS);
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
            Map<String, Object> row = Json.toDict(item);
            String token = firstStr(row, "token", "id").trim();
            if (token.isEmpty()) {
                result.add(Normalizer.normalizeEmail(flattenMessage(row, value), value));
                continue;
            }
            // 尝试获取邮件详情
            try {
                HttpResult detailResp = HttpClient.get(
                        BASE_URL + "/v1/email/" + ProviderUtil.urlEncode(token), HEADERS);
                detailResp.ensureSuccess();
                JsonElement detailParsed = Json.parse(detailResp.getBody());
                if (detailParsed != null && detailParsed.isJsonObject()) {
                    JsonElement emailEl = detailParsed.getAsJsonObject().get("email");
                    Map<String, Object> nested = (emailEl != null && emailEl.isJsonObject())
                            ? Json.toDict(emailEl) : Json.toDict(detailParsed);
                    result.add(Normalizer.normalizeEmail(flattenMessage(nested, value), value));
                } else {
                    result.add(Normalizer.normalizeEmail(flattenMessage(row, value), value));
                }
            } catch (RuntimeException ignored) {
                result.add(Normalizer.normalizeEmail(flattenMessage(row, value), value));
            }
        }
        return result;
    }
}
