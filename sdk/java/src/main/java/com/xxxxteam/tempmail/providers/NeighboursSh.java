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
 * neighbours.sh 渠道实现（固定域名 neighbours.sh）。
 *
 * <p>公共收件箱模式，无需注册，本地生成随机地址即可收信。</p>
 */
public final class NeighboursSh {

    private static final String CHANNEL = "neighbours-sh";
    private static final String API_BASE = "https://neighbours.sh/api/v1";
    private static final String DOMAIN = "neighbours.sh";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
    }

    private NeighboursSh() {
    }

    /**
     * 创建 neighbours.sh 临时邮箱（本地生成随机地址）。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String email = "sdk" + ProviderUtil.randomString(10) + "@" + DOMAIN;
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
            throw new RuntimeException("neighbours-sh: 缺少邮箱地址");
        }
        JsonObject data = requestJson("/inbox/" + ProviderUtil.urlEncode(address), true);
        if (data == null) {
            return new ArrayList<>();
        }
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
            String uid = Json.str(row, "uid").trim();
            if (uid.isEmpty()) {
                continue;
            }
            JsonObject detail = fetchDetail(address, uid);
            if (detail == null) {
                continue;
            }
            result.add(Normalizer.normalizeEmail(flattenMessage(detail, address), address));
        }
        return result;
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

    private static Map<String, Object> flattenMessage(JsonObject detail, String recipient) {
        Map<String, Object> dict = new LinkedHashMap<>();
        dict.put("id", Json.str(detail, "uid"));
        dict.put("from", firstAddress(detail.get("from")));
        String to = firstAddress(detail.get("to"));
        dict.put("to", to.isEmpty() ? recipient : to);
        dict.put("subject", Json.str(detail, "subject"));
        dict.put("text", Json.str(detail, "text"));
        dict.put("html", Json.str(detail, "html"));
        dict.put("date", Json.str(detail, "date"));
        dict.put("is_read", false);
        return dict;
    }
}