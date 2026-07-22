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
 * InboxKitten 渠道（inboxkitten.com/api/v1/mail）。
 */
public final class InboxKitten {

    private static final String API_BASE = "https://inboxkitten.com/api/v1/mail";
    private static final String DOMAIN = "inboxkitten.com";
    private static final Map<String, String> HEADERS_JSON = new LinkedHashMap<>();
    private static final Map<String, String> HEADERS_HTML = new LinkedHashMap<>();

    static {
        HEADERS_JSON.put("Accept", "application/json");
        HEADERS_HTML.put("Accept", "text/html,*/*");
    }

    private InboxKitten() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        return new EmailInfo("inboxkitten", ProviderUtil.randomLocalSdk() + "@" + DOMAIN);
    }

    /**
     * 获取邮件列表，逐封拉取正文。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        String local = (email != null ? email : "").trim().split("@")[0];
        if (local.isEmpty()) {
            throw new RuntimeException("inboxkitten: empty email");
        }
        HttpResult resp = HttpClient.get(
                API_BASE + "/list?recipient=" + ProviderUtil.urlEncode(local), HEADERS_JSON);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonArray()) {
            return result;
        }
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (item.isJsonObject()) {
                result.add(Normalizer.normalizeEmail(detailRaw(item.getAsJsonObject(), email), email));
            }
        }
        return result;
    }

    private static String getObjStr(JsonObject o, String key) {
        if (o != null && o.has(key) && o.get(key).isJsonPrimitive()) {
            return o.get(key).getAsString();
        }
        return "";
    }

    private static Map<String, Object> detailRaw(JsonObject row, String recipient) {
        JsonObject storage = row.has("storage") && row.get("storage").isJsonObject()
                ? row.getAsJsonObject("storage") : null;
        String region = getObjStr(storage, "region").trim();
        String key = getObjStr(storage, "key").trim();
        JsonObject message = row.has("message") && row.get("message").isJsonObject()
                ? row.getAsJsonObject("message") : null;
        JsonObject headers = message != null && message.has("headers") && message.get("headers").isJsonObject()
                ? message.getAsJsonObject("headers") : null;
        JsonObject envelope = row.has("envelope") && row.get("envelope").isJsonObject()
                ? row.getAsJsonObject("envelope") : null;

        Map<String, Object> raw = new LinkedHashMap<>();
        raw.put("id", key);
        raw.put("messageId", key);
        raw.put("from", ProviderUtil.firstNonEmpty(getObjStr(headers, "from"), getObjStr(envelope, "sender")));
        raw.put("to", ProviderUtil.firstNonEmpty(getObjStr(row, "recipient"), recipient));
        raw.put("subject", getObjStr(headers, "subject"));
        if (region.isEmpty() || key.isEmpty()) {
            return raw;
        }
        try {
            HttpResult metaResp = HttpClient.get(API_BASE + "/getKey?region=" + ProviderUtil.urlEncode(region)
                    + "&key=" + ProviderUtil.urlEncode(key), HEADERS_JSON);
            metaResp.ensureSuccess();
            JsonObject meta = Json.parseObject(metaResp.getBody());
            HttpResult htmlResp = HttpClient.get(API_BASE + "/getHtml?region=" + ProviderUtil.urlEncode(region)
                    + "&key=" + ProviderUtil.urlEncode(key), HEADERS_HTML);
            htmlResp.ensureSuccess();
            String html = htmlResp.getBody();
            raw.put("from", ProviderUtil.firstNonEmpty(getObjStr(meta, "name"), (String) raw.get("from")));
            raw.put("subject", ProviderUtil.firstNonEmpty(getObjStr(meta, "subject"), (String) raw.get("subject")));
            raw.put("text", Normalizer.htmlToText(html));
            raw.put("html", html);
        } catch (RuntimeException ignored) {
            // 保留摘要
        }
        return raw;
    }
}
