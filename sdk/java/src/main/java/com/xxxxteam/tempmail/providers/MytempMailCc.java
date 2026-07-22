package com.xxxxteam.tempmail.providers;

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
 * MyTempMail.cc 渠道 — https://mytempmail.cc
 *
 * <p>POST /api/address 创建邮箱，GET /api/mails/{token} 获取邮件。</p>
 */
public final class MytempMailCc {

    private static final String BASE_URL = "https://api.mytempmail.cc";
    private static final String DEFAULT_DOMAIN = "nilvaro.com";

    private MytempMailCc() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Content-Type", "application/json");
        h.put("Accept", "application/json");
        h.put("User-Agent", "Mozilla/5.0");
        return h;
    }

    /**
     * 创建 mytempmail.cc 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String name = ProviderUtil.randomString(10);
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("domain", DEFAULT_DOMAIN);
        body.put("name", name);
        body.put("expiry", 600);

        HttpResult resp = HttpClient.post(BASE_URL + "/api/address",
                Json.serialize(body), "application/json", headers());
        resp.ensureSuccess();

        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) throw new RuntimeException("mytempmail-cc: 响应格式无效");

        String token = Json.str(data, "token");
        String address = Json.str(data, "address");
        if (token.isEmpty() || address.isEmpty() || !address.contains("@")) {
            throw new RuntimeException("mytempmail-cc: 创建邮箱失败");
        }

        return new EmailInfo("mytempmail-cc", address, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token API token
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) throw new RuntimeException("mytempmail-cc: token 为空");

        HttpResult resp = HttpClient.get(BASE_URL + "/api/mails/" + ProviderUtil.urlEncode(token), headers());
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();
        var results = Json.arr(body, "results");
        if (results == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : results) {
            if (item.isJsonObject()) {
                out.add(Normalizer.normalizeEmail(Json.toDict(item), email));
            }
        }
        return out;
    }
}
