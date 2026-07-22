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
 * tempmail.lol V1 渠道（GET /generate）。
 */
public final class TempMailLolV2 {

    private static final String API_BASE = "https://api.tempmail.lol";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36");
    }

    private TempMailLolV2() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(API_BASE + "/generate", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String address = Json.str(data, "address");
        String token = Json.str(data, "token");
        if (address.isEmpty() || token.isEmpty()) {
            throw new RuntimeException("tempmail-lol-v2: missing address or token");
        }
        return new EmailInfo("tempmail-lol-v2", address, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token 会话令牌
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        HttpResult resp = HttpClient.get(
                API_BASE + "/auth/" + ProviderUtil.urlEncode(token != null ? token : ""), HEADERS);
        resp.ensureSuccess();
        JsonArray list = Json.arr(Json.parse(resp.getBody()), "email");
        List<Email> result = new ArrayList<>();
        if (list == null) {
            return result;
        }
        for (JsonElement item : list) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject raw = item.getAsJsonObject();
            String body = Json.str(raw, "body");
            Map<String, Object> dict = new LinkedHashMap<>();
            dict.put("id", !Json.str(raw, "id").isEmpty() ? Json.str(raw, "id") : Json.str(raw, "_id"));
            dict.put("from", !Json.str(raw, "from").isEmpty() ? Json.str(raw, "from") : Json.str(raw, "sender"));
            dict.put("to", email);
            dict.put("subject", Json.str(raw, "subject"));
            dict.put("text", !body.isEmpty() ? body : Json.str(raw, "text"));
            dict.put("html", !Json.str(raw, "html").isEmpty() ? Json.str(raw, "html") : body);
            dict.put("date", !Json.str(raw, "date").isEmpty() ? Json.str(raw, "date") : Json.str(raw, "receivedAt"));
            result.add(Normalizer.normalizeEmail(dict, email));
        }
        return result;
    }
}
