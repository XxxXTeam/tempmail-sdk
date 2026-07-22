package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
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
 * tempmail.lol V2 渠道（POST /inbox/create）。支持指定域名。
 */
public final class TempMailLol {

    private static final String BASE_URL = "https://api.tempmail.lol/v2";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36");
        HEADERS.put("Origin", "https://tempmail.lol");
    }

    private TempMailLol() {
    }

    /**
     * 创建临时邮箱。
     *
     * @param domain 指定域名，可为 null
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("domain", domain);
        body.put("captcha", null);
        String payload = Json.serialize(body);
        HttpResult resp = HttpClient.post(BASE_URL + "/inbox/create", payload, "application/json", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String address = Json.str(data, "address");
        String token = Json.str(data, "token");
        if (address.isEmpty() || token.isEmpty()) {
            throw new RuntimeException("Failed to generate email");
        }
        return new EmailInfo("tempmail-lol", address, token, null, null);
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
                BASE_URL + "/inbox?token=" + ProviderUtil.urlEncode(token != null ? token : ""), HEADERS);
        resp.ensureSuccess();
        JsonArray emails = Json.arr(Json.parse(resp.getBody()), "emails");
        List<Email> result = new ArrayList<>();
        if (emails == null) {
            return result;
        }
        for (JsonElement raw : emails) {
            if (raw != null && !raw.isJsonNull()) {
                result.add(Normalizer.normalizeEmail(Json.toDict(raw), email));
            }
        }
        return result;
    }
}
