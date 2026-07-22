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
 * Neighbours 渠道实现 — https://neighbours.sh（多域名）。
 *
 * <p>与 NeighboursSh 共用同一 API，区别在于支持动态域名列表。</p>
 */
public final class Neighbours {

    private static final String CHANNEL = "neighbours";
    private static final String API_BASE = "https://neighbours.sh/api/v1";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
    }

    private Neighbours() {
    }

    /**
     * 创建临时邮箱（支持指定域名）。
     *
     * @param domain 指定域名，null 或空串则随机选取
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        List<String> domains = listDomains();
        String selected;
        if (domain != null && !domain.trim().isEmpty()) {
            String wanted = domain.trim().toLowerCase();
            if (!domains.contains(wanted)) {
                throw new RuntimeException("neighbours: 不支持的域名 " + domain);
            }
            selected = wanted;
        } else {
            selected = domains.get(ThreadLocalRandom.current().nextInt(domains.size()));
        }
        String email = "sdk" + ProviderUtil.randomString(16) + "@" + selected;
        return new EmailInfo(CHANNEL, email, email, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("neighbours: 邮箱地址为空");
        }
        JsonObject data = requestJson("/inbox/" + ProviderUtil.urlEncode(address), true);
        if (data == null) {
            return new ArrayList<>();
        }
        JsonElement rowsEl = data.get("data");
        if (rowsEl == null || !rowsEl.isJsonArray()) {
            return new ArrayList<>();
        }
        JsonArray rows = rowsEl.getAsJsonArray();
        List<Email> result = new ArrayList<>();
        for (JsonElement item : rows) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject row = item.getAsJsonObject();
            String uid = Json.str(row, "uid").trim();
            JsonObject detail = uid.isEmpty() ? null : fetchDetail(address, uid);
            Map<String, Object> flat = flattenMessage(detail != null ? detail : row, address);
            result.add(Normalizer.normalizeEmail(flat, address));
        }
        return result;
    }

    private static List<String> listDomains() {
        JsonObject data = requestJson("/config/domains", false);
        List<String> domains = new ArrayList<>();
        if (data != null) {
            JsonElement nested = data.get("data");
            if (nested != null && nested.isJsonObject()) {
                JsonArray arr = Json.arr(nested, "domains");
                if (arr != null) {
                    for (JsonElement el : arr) {
                        if (el.isJsonPrimitive()) {
                            String d = el.getAsString().trim().toLowerCase();
                            if (!d.isEmpty()) {
                                domains.add(d);
                            }
                        }
                    }
                }
            }
        }
        if (domains.isEmpty()) {
            throw new RuntimeException("neighbours: 域名列表为空");
        }
        return domains;
    }

    private static JsonObject requestJson(String path, boolean allow404) {
        HttpResult resp = HttpClient.get(API_BASE + path, HEADERS);
        if (allow404 && resp.getStatusCode() == 404) {
            return null;
        }
        resp.ensureSuccess();
        JsonElement el = Json.parse(resp.getBody());
        return (el != null && el.isJsonObject()) ? el.getAsJsonObject() : null;
    }

    private static JsonObject fetchDetail(String address, String uid) {
        JsonObject data = requestJson(
                "/inbox/" + ProviderUtil.urlEncode(address) + "/" + ProviderUtil.urlEncode(uid), true);
        if (data == null) {
            return null;
        }
        JsonElement detailEl = data.get("data");
        return (detailEl != null && detailEl.isJsonObject()) ? detailEl.getAsJsonObject() : null;
    }

    private static String firstAddress(JsonElement value) {
        if (value == null || value.isJsonNull()) {
            return "";
        }
        if (value.isJsonPrimitive()) {
            return value.getAsString().trim();
        }
        if (value.isJsonArray()) {
            for (JsonElement item : value.getAsJsonArray()) {
                String hit = firstAddress(item);
                if (!hit.isEmpty()) {
                    return hit;
                }
            }
            return "";
        }
        if (value.isJsonObject()) {
            JsonObject obj = value.getAsJsonObject();
            String addr = Json.str(obj, "address").trim();
            if (!addr.isEmpty()) {
                return addr;
            }
            String text = Json.str(obj, "text").trim();
            if (text.contains("@")) {
                return text;
            }
            return firstAddress(obj.get("value"));
        }
        return "";
    }

    private static Map<String, Object> flattenMessage(JsonObject raw, String recipient) {
        String text = ProviderUtil.firstNonEmpty(Json.str(raw, "text"), Json.str(raw, "snippet"));
        String date = ProviderUtil.firstNonEmpty(Json.str(raw, "date"), Json.str(raw, "received_at"));
        Map<String, Object> dict = new LinkedHashMap<>();
        dict.put("id", Json.str(raw, "uid"));
        dict.put("from", firstAddress(raw.get("from")));
        String to = firstAddress(raw.get("to"));
        dict.put("to", to.isEmpty() ? recipient : to);
        dict.put("subject", Json.str(raw, "subject"));
        dict.put("text", text);
        dict.put("html", Json.str(raw, "html"));
        dict.put("date", date);
        dict.put("is_read", false);
        return dict;
    }
}
