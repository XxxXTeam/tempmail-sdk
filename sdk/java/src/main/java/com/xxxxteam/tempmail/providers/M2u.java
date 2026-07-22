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
 * M2u (MailToYou) 渠道（m2u.io）。
 * Token 为 JSON 字符串 {"token":"...","viewToken":"..."}。
 */
public final class M2u {

    private static final String API_BASE = "https://api.m2u.io";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36");
    }

    private M2u() {
    }

    /**
     * 打包 token 和 viewToken 为 JSON 字符串。
     */
    private static String packToken(String token, String viewToken) {
        Map<String, String> m = new LinkedHashMap<>();
        m.put("token", token);
        m.put("viewToken", viewToken);
        return Json.serialize(m);
    }

    /**
     * 从打包的 token 字符串中解包。
     *
     * @return [token, viewToken]
     */
    private static String[] unpackToken(String packed) {
        if (packed == null || packed.isBlank()) {
            return new String[]{"", ""};
        }
        String v = packed.trim();
        if (v.startsWith("{")) {
            JsonElement el = Json.parse(v);
            if (el != null && el.isJsonObject()) {
                return new String[]{
                        Json.str(el, "token").trim(),
                        Json.str(el, "viewToken").trim()
                };
            }
        }
        return new String[]{v, ""};
    }

    private static Map<String, Object> flattenMessage(Map<String, Object> raw, String recipient) {
        Map<String, Object> out = new LinkedHashMap<>();
        out.put("id", firstStr(raw, "id", "message_id"));
        out.put("from", firstStr(raw, "from_addr", "from_address", "from"));
        out.put("to", recipient);
        out.put("subject", firstStr(raw, "subject"));
        out.put("text", firstStr(raw, "text_body", "body_text", "text"));
        out.put("html", firstStr(raw, "html_body", "body_html", "html"));
        out.put("date", firstStr(raw, "received_at", "created_at"));
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
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> postHeaders = new LinkedHashMap<>(HEADERS);
        postHeaders.put("Content-Type", "application/json");
        HttpResult resp = HttpClient.post(API_BASE + "/v1/mailboxes/auto", "{}", "application/json", postHeaders);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("m2u: invalid mailbox response");
        }
        JsonElement mailboxEl = parsed.getAsJsonObject().get("mailbox");
        if (mailboxEl == null || !mailboxEl.isJsonObject()) {
            throw new RuntimeException("m2u: invalid mailbox response");
        }
        String localPart = Json.str(mailboxEl, "local_part").trim();
        String domain = Json.str(mailboxEl, "domain").trim();
        String token = Json.str(mailboxEl, "token").trim();
        String viewToken = Json.str(mailboxEl, "view_token").trim();
        String email = (!localPart.isEmpty() && !domain.isEmpty()) ? localPart + "@" + domain : "";
        if (email.isEmpty() || token.isEmpty() || viewToken.isEmpty()) {
            throw new RuntimeException("m2u: invalid mailbox response");
        }
        String expiresAt = Json.str(mailboxEl, "expires_at").trim();
        String createdAt = Json.str(mailboxEl, "created_at").trim();
        return new EmailInfo("m2u", email, packToken(token, viewToken), null,
                createdAt.isEmpty() ? null : createdAt);
    }

    /**
     * 获取邮件列表。
     *
     * @param token  打包的 token JSON
     * @param email  邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String[] unpacked = unpackToken(token);
        String mailboxToken = unpacked[0];
        String viewToken = unpacked[1];
        String address = (email != null ? email : "").trim();
        if (mailboxToken.isEmpty()) {
            throw new RuntimeException("m2u: missing token");
        }
        if (viewToken.isEmpty()) {
            throw new RuntimeException("m2u: missing view token");
        }
        if (address.isEmpty()) {
            throw new RuntimeException("m2u: empty email");
        }
        HttpResult resp = HttpClient.get(
                API_BASE + "/v1/mailboxes/" + ProviderUtil.urlEncode(mailboxToken)
                        + "/messages?view=" + ProviderUtil.urlEncode(viewToken), HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonObject()) {
            return result;
        }
        JsonElement rowsEl = parsed.getAsJsonObject().get("messages");
        if (rowsEl == null || !rowsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement item : rowsEl.getAsJsonArray()) {
            if (!item.isJsonObject()) {
                continue;
            }
            Map<String, Object> row = Json.toDict(item);
            String messageId = firstStr(row, "id", "message_id").trim();
            Map<String, Object> detail = messageId.isEmpty() ? null
                    : fetchDetail(mailboxToken, viewToken, messageId);
            result.add(Normalizer.normalizeEmail(flattenMessage(detail != null ? detail : row, address), address));
        }
        return result;
    }

    private static Map<String, Object> fetchDetail(String mailboxToken, String viewToken, String messageId) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/v1/mailboxes/" + ProviderUtil.urlEncode(mailboxToken)
                            + "/messages/" + ProviderUtil.urlEncode(messageId)
                            + "?view=" + ProviderUtil.urlEncode(viewToken), HEADERS);
            if (resp.getStatusCode() < 200 || resp.getStatusCode() >= 300) {
                return null;
            }
            JsonElement parsed = Json.parse(resp.getBody());
            if (parsed != null && parsed.isJsonObject()) {
                JsonElement msgEl = parsed.getAsJsonObject().get("message");
                if (msgEl != null && msgEl.isJsonObject()) {
                    return Json.toDict(msgEl);
                }
            }
        } catch (RuntimeException ignored) {
        }
        return null;
    }
}
