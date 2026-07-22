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
 * mail.cx 匿名 Web API 渠道（mail.cx/v1）。ddker.com 复用。
 */
public final class MailCx {

    private static final String BASE_URL = "https://mail.cx";

    private MailCx() {
    }

    private static Map<String, String> buildHeaders(String clientId) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json");
        h.put("Origin", BASE_URL);
        h.put("Referer", BASE_URL + "/");
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        h.put("X-Client-ID", clientId);
        return h;
    }

    /**
     * 创建邮箱。preferredDomain 指定域名（可为 null），channel 为对外标识。
     *
     * @param channel         渠道标识
     * @param preferredDomain 指定域名，可为 null
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel, String preferredDomain) {
        String clientId = "tempmail-sdk-" + ProviderUtil.randomString(16);
        HttpResult resp = HttpClient.get(BASE_URL + "/v1/config", buildHeaders(clientId));
        resp.ensureSuccess();
        JsonObject config = Json.parseObject(resp.getBody());
        String domain = pickDomain(config, preferredDomain);
        return new EmailInfo(channel, ProviderUtil.randomString(12) + "@" + domain, clientId, null, null);
    }

    private static String pickDomain(JsonObject config, String preferred) {
        JsonArray items = config != null && config.has("system_domains") && config.get("system_domains").isJsonArray()
                ? config.getAsJsonArray("system_domains") : null;
        List<String> domains = new ArrayList<>();
        if (items != null) {
            for (JsonElement it : items) {
                if (it.isJsonObject()) {
                    String d = Json.str(it.getAsJsonObject(), "domain").trim();
                    if (!d.isEmpty()) {
                        domains.add(d);
                    }
                }
            }
        }
        if (domains.isEmpty()) {
            throw new RuntimeException("mail-cx: no system domains");
        }
        String want = (preferred != null ? preferred : "").trim();
        if (want.startsWith("@")) {
            want = want.substring(1);
        }
        want = want.toLowerCase();
        if (!want.isEmpty()) {
            for (String d : domains) {
                if (d.toLowerCase().equals(want)) {
                    return d;
                }
            }
        }
        if (items != null) {
            for (JsonElement it : items) {
                if (it.isJsonObject()) {
                    JsonObject o = it.getAsJsonObject();
                    boolean def = o.has("default") && o.get("default").isJsonPrimitive()
                            && o.get("default").getAsBoolean();
                    if (def) {
                        String d = Json.str(o, "domain").trim();
                        if (!d.isEmpty()) {
                            return d;
                        }
                    }
                }
            }
        }
        return domains.get(0);
    }

    /**
     * 获取邮件列表，逐封拉取详情。
     *
     * @param clientId 客户端 ID（作为 token 存储）
     * @param email    邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String clientId, String email) {
        HttpResult resp = HttpClient.get(
                BASE_URL + "/v1/inbox/" + ProviderUtil.urlEncode(email), buildHeaders(clientId != null ? clientId : ""));
        if (resp.getStatusCode() == 204) {
            return new ArrayList<>();
        }
        resp.ensureSuccess();
        JsonArray rows = Json.arr(Json.parse(resp.getBody()), "emails");
        List<Email> result = new ArrayList<>();
        if (rows == null) {
            return result;
        }
        for (JsonElement row : rows) {
            if (!row.isJsonObject()) {
                result.add(Normalizer.normalizeEmail(new LinkedHashMap<>(), email));
                continue;
            }
            JsonObject ro = row.getAsJsonObject();
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(ro, "id"));
            raw.put("from", ProviderUtil.firstNonEmpty(Json.str(ro, "from_email"), Json.str(ro, "from_name")));
            raw.put("to", email);
            raw.put("subject", Json.str(ro, "subject"));
            raw.put("text", Json.str(ro, "preview_text"));
            raw.put("created_at", Json.str(ro, "created_at"));
            String id = Json.str(ro, "id");
            if (!id.isEmpty()) {
                try {
                    HttpResult dr = HttpClient.get(
                            BASE_URL + "/v1/email/" + ProviderUtil.urlEncode(id), buildHeaders(clientId != null ? clientId : ""));
                    if (dr.isOk()) {
                        JsonObject det = Json.parseObject(dr.getBody());
                        if (det != null) {
                            raw = new LinkedHashMap<>();
                            raw.put("id", Json.str(det, "id"));
                            raw.put("from", ProviderUtil.firstNonEmpty(Json.str(det, "from_email"), Json.str(det, "from_name")));
                            raw.put("to", email);
                            raw.put("subject", Json.str(det, "subject"));
                            raw.put("text", ProviderUtil.firstNonEmpty(Json.str(det, "text_body"), Json.str(det, "preview_text")));
                            raw.put("html", Json.str(det, "html_body"));
                            raw.put("created_at", Json.str(det, "created_at"));
                        }
                    }
                } catch (RuntimeException ignored) {
                    // 回退列表摘要
                }
            }
            result.add(Normalizer.normalizeEmail(raw, email));
        }
        return result;
    }
}
