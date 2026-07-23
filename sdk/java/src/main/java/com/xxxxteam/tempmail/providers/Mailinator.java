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
 * Mailinator 及其姊妹域名渠道（mailinator.com，domain=public 公共收件箱）。
 */
public final class Mailinator {

    private static final String BASE_URL = "https://mailinator.com";
    private static final String PUBLIC_DOMAIN = "public";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    }

    private Mailinator() {
    }

    /**
     * 创建邮箱：指定域名（默认 mailinator.com），渠道标识可为姊妹域名。
     *
     * @param channel 渠道标识
     * @param domain  邮箱域名
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel, String domain) {
        return new EmailInfo(channel, ProviderUtil.randomString(12) + "@" + domain);
    }

    /**
     * 使用默认渠道与默认域名创建邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        return generate("mailinator", "mailinator.com");
    }

    /**
     * 获取邮件列表，逐封拉取纯文本与 HTML 正文。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        String inbox = email != null ? email.trim() : "";
        int at = inbox.indexOf('@');
        if (at >= 0) {
            inbox = inbox.substring(0, at);
        }
        if (inbox.isEmpty()) {
            return new ArrayList<>();
        }

        JsonObject listData = requestJson(BASE_URL + "/api/v2/domains/" + PUBLIC_DOMAIN + "/inboxes/" + inbox);
        List<JsonObject> messages = parseMessages(listData);
        List<Email> result = new ArrayList<>();
        for (JsonObject msg : messages) {
            String id = ProviderUtil.firstNonEmpty(Json.str(msg, "id"), Json.str(msg, "messageId"));
            JsonObject textPayload = null;
            JsonObject htmlPayload = null;
            JsonObject attachmentsPayload = null;
            if (!id.isEmpty()) {
                textPayload = requestJson(BASE_URL + "/api/v2/domains/" + PUBLIC_DOMAIN + "/messages/" + id + "/text");
                htmlPayload = requestJson(BASE_URL + "/api/v2/domains/" + PUBLIC_DOMAIN + "/messages/" + id + "/texthtml");
                attachmentsPayload = requestJson(BASE_URL + "/api/v2/domains/" + PUBLIC_DOMAIN + "/messages/" + id + "/attachments");
            }
            // 解析附件列表
            List<Map<String, Object>> attachments = new ArrayList<>();
            if (attachmentsPayload != null && attachmentsPayload.has("attachments")
                    && attachmentsPayload.get("attachments").isJsonArray()) {
                for (JsonElement attEl : attachmentsPayload.getAsJsonArray("attachments")) {
                    if (!attEl.isJsonObject()) {
                        continue;
                    }
                    JsonObject att = attEl.getAsJsonObject();
                    Map<String, Object> attMap = new LinkedHashMap<>();
                    attMap.put("filename", ProviderUtil.firstNonEmpty(Json.str(att, "name"), Json.str(att, "filename")));
                    String attUrl = ProviderUtil.firstNonEmpty(Json.str(att, "downloadUrl"), Json.str(att, "url"));
                    if (!attUrl.isEmpty() && !attUrl.startsWith("http://") && !attUrl.startsWith("https://")) {
                        attUrl = BASE_URL + attUrl;
                    }
                    attMap.put("url", attUrl);
                    attMap.put("contentType", ProviderUtil.firstNonEmpty(
                            Json.str(att, "contentType"), Json.str(att, "content_type"),
                            Json.str(att, "mimeType"), Json.str(att, "mime_type")));
                    String sizeStr = ProviderUtil.firstNonEmpty(Json.str(att, "size"), Json.str(att, "filesize"));
                    if (!sizeStr.isEmpty()) {
                        try {
                            attMap.put("size", Long.parseLong(sizeStr));
                        } catch (NumberFormatException ignored) {
                            // 忽略非数字 size
                        }
                    }
                    attachments.add(attMap);
                }
            }
            // 优先读 "text" 键（/text 端点实际返回键），回退兜底 "text/plain"（防御性编程）
            String textContent = textPayload == null ? "" : Json.str(textPayload, "text");
            if (textContent.isEmpty() && textPayload != null) {
                textContent = Json.str(textPayload, "text/plain");
            }
            // 优先读 "html" 键，回退兜底 "text/html"
            String htmlContent = htmlPayload == null ? "" : Json.str(htmlPayload, "html");
            if (htmlContent.isEmpty() && htmlPayload != null) {
                htmlContent = Json.str(htmlPayload, "text/html");
            }

            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", id);
            raw.put("from", ProviderUtil.firstNonEmpty(Json.str(msg, "from"), Json.str(msg, "origfrom")));
            raw.put("to", Json.str(msg, "to").isEmpty() ? email : Json.str(msg, "to"));
            raw.put("subject", Json.str(msg, "subject"));
            raw.put("text", textContent);
            raw.put("html", htmlContent);
            raw.put("date", ProviderUtil.firstNonEmpty(Json.str(msg, "time"), Json.str(msg, "date")));
            raw.put("attachments", attachments);
            result.add(Normalizer.normalizeEmail(raw, email));
        }
        return result;
    }

    private static JsonObject requestJson(String url) {
        try {
            HttpResult resp = HttpClient.get(url, HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException e) {
            return null;
        }
    }

    private static List<JsonObject> parseMessages(JsonObject data) {
        List<JsonObject> result = new ArrayList<>();
        if (data == null) {
            return result;
        }
        for (String key : new String[] {"msgs", "data"}) {
            if (data.has(key) && data.get(key).isJsonArray()) {
                JsonArray arr = data.getAsJsonArray(key);
                for (JsonElement it : arr) {
                    if (it.isJsonObject()) {
                        result.add(it.getAsJsonObject());
                    }
                }
                return result;
            }
        }
        return result;
    }
}
