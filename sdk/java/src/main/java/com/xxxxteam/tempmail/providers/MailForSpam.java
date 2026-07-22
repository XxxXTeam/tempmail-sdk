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
 * MailForSpam 渠道（mailforspam.com）。
 *
 * <p>域名别名：mailforspam.com / tempmail.io / disposable.email</p>
 */
public final class MailForSpam {

    private static final String API_BASE = "https://api.mailforspam.com/api";
    private static final String DEFAULT_DOMAIN = "mailforspam.com";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("Referer", "https://mailforspam.com/");
        HEADERS.put("Origin", "https://mailforspam.com");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    }

    private MailForSpam() {
    }

    /**
     * 创建临时邮箱。
     *
     * @param channel 渠道标识
     * @param domain  指定域名，可为 null（默认 mailforspam.com）
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel, String domain) {
        String selectedDomain = pickDomain(domain);
        String email = ProviderUtil.randomLocalSdk() + "@" + selectedDomain;

        HttpResult resp = HttpClient.get(mailboxUrl(email, 1), HEADERS);
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
            throw new IllegalArgumentException("mailforspam: empty email");
        }
        String address = email.trim();

        HttpResult resp = HttpClient.get(mailboxUrl(address, 100), HEADERS);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) {
            return new ArrayList<>();
        }
        JsonArray rows = Json.arr(data, "emails");
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
            JsonObject detail = fetchMessage(messageId, address);
            JsonObject source = detail != null ? detail : item;
            result.add(Normalizer.normalizeEmail(flatten(source, address), address));
        }
        return result;
    }

    private static String mailboxUrl(String email, int pageSize) {
        return API_BASE + "/mailboxes/" + ProviderUtil.urlEncode(email)
                + "/emails?page=1&page_size=" + pageSize + "&sort_by=date&sort_order=desc";
    }

    private static JsonObject fetchMessage(String messageId, String email) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/mailboxes/" + ProviderUtil.urlEncode(email)
                            + "/emails/" + ProviderUtil.urlEncode(messageId),
                    HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException ignored) {
            return null;
        }
    }

    private static Map<String, Object> flatten(JsonObject raw, String recipient) {
        Map<String, Object> flat = new LinkedHashMap<>();
        flat.put("id", Json.str(raw, "id"));
        flat.put("from", Json.str(raw, "sender"));
        String receiver = Json.str(raw, "receiver");
        flat.put("to", receiver.isEmpty() ? recipient : receiver);
        flat.put("subject", Json.str(raw, "subject"));
        flat.put("text", Json.str(raw, "body_text"));
        flat.put("html", Json.str(raw, "body_html"));
        flat.put("date", Json.str(raw, "date"));
        String readAt = Json.str(raw, "readAt");
        flat.put("isRead", !readAt.isEmpty());
        return flat;
    }

    private static String pickDomain(String preferred) {
        String p = (preferred != null ? preferred : "").trim();
        if (p.startsWith("@")) {
            p = p.substring(1);
        }
        p = p.toLowerCase();
        if (!p.isEmpty()) {
            for (String domain : new String[]{"mailforspam.com", "tempmail.io", "disposable.email"}) {
                if (domain.equals(p)) {
                    return domain;
                }
            }
        }
        return DEFAULT_DOMAIN;
    }
}
