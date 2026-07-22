package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * ta-easy.com 临时邮箱渠道。
 * POST /temp-email/address/new 创建邮箱，POST /temp-email/inbox/list 获取邮件。
 */
public final class TaEasy {

    private static final String API_BASE = "https://api-endpoint.ta-easy.com";
    private static final String ORIGIN = "https://www.ta-easy.com";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        HEADERS.put("Origin", ORIGIN);
        HEADERS.put("Referer", ORIGIN + "/");
    }

    private TaEasy() {
    }

    /**
     * 创建 ta-easy 临时邮箱（POST /temp-email/address/new，空 body）。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> h = new LinkedHashMap<>(HEADERS);
        h.put("Content-Length", "0");
        HttpResult resp = HttpClient.post(API_BASE + "/temp-email/address/new", "", null, h);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String status = Json.str(data, "status");
        String address = Json.str(data, "address");
        String token = Json.str(data, "token");
        if (!"success".equals(status) || address.isEmpty() || token.isEmpty()) {
            throw new RuntimeException("ta-easy: 创建邮箱失败");
        }
        return new EmailInfo("ta-easy", address, token, null, null);
    }

    /**
     * 获取邮件列表（POST /temp-email/inbox/list）。
     *
     * @param email 邮箱地址
     * @param token 认证令牌
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String addr = (email != null ? email : "").trim();
        String tok = (token != null ? token : "").trim();
        if (addr.isEmpty()) {
            throw new RuntimeException("ta-easy: 邮箱地址为空");
        }
        if (tok.isEmpty()) {
            throw new RuntimeException("ta-easy: token 为空");
        }
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("token", tok);
        body.put("email", addr);
        String payload = Json.serialize(body);
        Map<String, String> h = new LinkedHashMap<>(HEADERS);
        h.put("Content-Type", "application/json");
        HttpResult resp = HttpClient.post(API_BASE + "/temp-email/inbox/list", payload, "application/json", h);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String status = Json.str(data, "status");
        if (!"success".equals(status)) {
            throw new RuntimeException("ta-easy: 获取邮件失败");
        }
        List<Email> result = new ArrayList<>();
        var arr = Json.arr(data, "data");
        if (arr == null) {
            return result;
        }
        for (JsonElement item : arr) {
            if (item != null && item.isJsonObject()) {
                result.add(Normalizer.normalizeEmail(Json.toDict(item), addr));
            }
        }
        return result;
    }
}
