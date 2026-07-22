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
import java.util.concurrent.ThreadLocalRandom;

/**
 * Temporam 渠道实现 — https://temporam.com
 */
public final class Temporam {

    private static final String ORIGIN = "https://temporam.com";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("Accept-Encoding", "gzip, deflate, br");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36");
    }

    private Temporam() {
    }

    /**
     * 创建临时邮箱（支持指定域名）。
     *
     * @param domain 指定域名，null 或空串则随机选取
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        List<String> domains = fetchDomains();
        String selected;
        if (domain != null && !domain.trim().isEmpty()) {
            if (!domains.contains(domain.trim())) {
                throw new RuntimeException("temporam: 不支持的域名 " + domain);
            }
            selected = domain.trim();
        } else {
            selected = domains.get(ThreadLocalRandom.current().nextInt(domains.size()));
        }
        String email = "sdk" + ProviderUtil.randomString(18) + "@" + selected;
        return new EmailInfo("temporam", email, email, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token 令牌（即邮箱地址本身）
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String recipient = (token != null && !token.isEmpty()) ? token : email;
        if (recipient == null || recipient.isEmpty()) {
            throw new RuntimeException("temporam: 缺少邮箱地址");
        }
        JsonObject data = getJson("/api/emails?email=" + ProviderUtil.urlEncode(recipient) + "&limit=50");
        JsonArray rows = Json.arr(data, "data");
        List<Email> result = new ArrayList<>();
        if (rows == null) {
            return result;
        }
        for (JsonElement item : rows) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject row = item.getAsJsonObject();
            String msgId = ProviderUtil.firstNonEmpty(Json.str(row, "id"), Json.str(row, "uuid"));
            JsonObject raw = row;
            if (!msgId.isEmpty()) {
                try {
                    JsonObject detail = getJson("/api/emails/" + ProviderUtil.urlEncode(msgId));
                    JsonElement dataEl = detail.get("data");
                    if (dataEl != null && dataEl.isJsonObject()) {
                        raw = dataEl.getAsJsonObject();
                    }
                } catch (RuntimeException ignored) {
                    // 回退到列表摘要
                }
            }
            result.add(Normalizer.normalizeEmail(flatten(raw, recipient), recipient));
        }
        return result;
    }

    private static List<String> fetchDomains() {
        JsonObject data = getJson("/api/domains");
        JsonArray arr = Json.arr(data, "data");
        List<String> domains = new ArrayList<>();
        if (arr != null) {
            for (JsonElement item : arr) {
                if (item.isJsonObject()) {
                    String d = Json.str(item, "domain").trim();
                    if (!d.isEmpty()) {
                        domains.add(d);
                    }
                }
            }
        }
        if (domains.isEmpty()) {
            throw new RuntimeException("temporam: 域名列表为空");
        }
        return domains;
    }

    private static JsonObject getJson(String path) {
        HttpResult resp = HttpClient.get(ORIGIN + path, HEADERS);
        resp.ensureSuccess();
        JsonElement el = Json.parse(resp.getBody());
        if (el == null || !el.isJsonObject()) {
            throw new RuntimeException("temporam: 无效的 JSON 响应");
        }
        return el.getAsJsonObject();
    }

    private static Map<String, Object> flatten(JsonObject raw, String email) {
        Map<String, Object> dict = new LinkedHashMap<>();
        dict.put("id", ProviderUtil.firstNonEmpty(Json.str(raw, "id"), Json.str(raw, "uuid")));
        dict.put("from", ProviderUtil.firstNonEmpty(Json.str(raw, "fromEmail"), Json.str(raw, "from")));
        String to = ProviderUtil.firstNonEmpty(Json.str(raw, "toEmail"), Json.str(raw, "to"));
        dict.put("to", to.isEmpty() ? email : to);
        dict.put("subject", Json.str(raw, "subject"));
        dict.put("text", Json.str(raw, "text"));
        dict.put("html", Json.str(raw, "html"));
        dict.put("date", ProviderUtil.firstNonEmpty(
                Json.str(raw, "createdAt"), Json.str(raw, "created_at"), Json.str(raw, "date")));
        return dict;
    }
}
