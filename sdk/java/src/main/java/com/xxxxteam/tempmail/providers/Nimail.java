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
 * nimail 渠道实现 — https://www.nimail.cn
 *
 * <p>简单 POST 表单 API 临时邮箱，无需认证，Token 即邮箱地址本身。</p>
 */
public final class Nimail {

    private static final String BASE_URL = "https://www.nimail.cn";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
        HEADERS.put("Content-Type", "application/x-www-form-urlencoded");
        HEADERS.put("Origin", BASE_URL);
        HEADERS.put("Referer", BASE_URL + "/");
    }

    private Nimail() {
    }

    /**
     * 创建 nimail.cn 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String email = ProviderUtil.randomString(10) + "@nimail.cn";
        String body = "mail=" + ProviderUtil.urlEncode(email);
        HttpResult resp = HttpClient.post(
                BASE_URL + "/api/applymail", body, "application/x-www-form-urlencoded", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonObject()) {
            throw new RuntimeException("nimail: 创建邮箱失败");
        }
        JsonObject root = data.getAsJsonObject();
        if (!"true".equals(Json.str(root, "success")) || Json.str(root, "user").isEmpty()) {
            throw new RuntimeException("nimail: 创建邮箱失败");
        }
        String user = Json.str(root, "user");
        return new EmailInfo("nimail", user, user, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        String addr = (email != null ? email : "").trim();
        if (addr.isEmpty()) {
            throw new RuntimeException("nimail: 缺少邮箱地址");
        }
        String body = "mail=" + ProviderUtil.urlEncode(addr) + "&time=0";
        HttpResult resp = HttpClient.post(
                BASE_URL + "/api/getmails", body, "application/x-www-form-urlencoded", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonObject()) {
            return new ArrayList<>();
        }
        JsonObject root = data.getAsJsonObject();
        if (!"true".equals(Json.str(root, "success"))) {
            return new ArrayList<>();
        }
        JsonArray mail = Json.arr(root, "mail");
        List<Email> result = new ArrayList<>();
        if (mail == null) {
            return result;
        }
        for (JsonElement item : mail) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject msg = item.getAsJsonObject();
            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", firstStr(msg, "id", "time"));
            row.put("from", firstStr(msg, "from", "sender"));
            row.put("to", addr);
            row.put("subject", firstStr(msg, "subject", "title"));
            row.put("text", firstStr(msg, "text", "content"));
            row.put("html", firstStr(msg, "html", "content"));
            row.put("date", firstStr(msg, "date", "time"));
            result.add(Normalizer.normalizeEmail(row, addr));
        }
        return result;
    }

    private static String firstStr(JsonObject msg, String... keys) {
        for (String key : keys) {
            String val = Json.str(msg, key);
            if (!val.isEmpty()) {
                return val;
            }
        }
        return "";
    }
}
