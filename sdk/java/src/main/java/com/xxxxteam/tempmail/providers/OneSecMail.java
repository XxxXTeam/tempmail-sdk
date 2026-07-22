package com.xxxxteam.tempmail.providers;

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
 * 1SecMail 渠道（tmaily.com，会话 Cookie 鉴权）。
 */
public final class OneSecMail {

    private static final String BASE_URL = "https://tmaily.com/";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0");
    }

    private OneSecMail() {
    }

    /**
     * 创建临时邮箱，提取会话 Cookie 作为 token。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(BASE_URL + "generate", HEADERS);
        resp.ensureSuccess();
        String cookie = extractCookie(resp);
        if (cookie.isEmpty()) {
            throw new RuntimeException("1sec-mail: 未获取到会话 Cookie");
        }
        String email = Json.str(Json.parse(resp.getBody()), "address").trim();
        if (!email.contains("@")) {
            throw new RuntimeException("1sec-mail: 无效的邮箱响应");
        }
        return new EmailInfo("1sec-mail", email, cookie, null, null);
    }

    private static String extractCookie(HttpResult resp) {
        for (String c : resp.getSetCookies()) {
            int idx = c.indexOf("TMaily_sid=");
            if (idx < 0) {
                continue;
            }
            String seg = c.substring(idx);
            int semi = seg.indexOf(';');
            return semi < 0 ? seg : seg.substring(0, semi);
        }
        return "";
    }

    /**
     * 获取邮件列表。
     *
     * @param token 会话 Cookie
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("1sec-mail: 邮箱地址为空");
        }
        Map<String, String> headers = new LinkedHashMap<>(HEADERS);
        headers.put("Cookie", token != null ? token : "");
        HttpResult resp = HttpClient.get(
                BASE_URL + "emails?address=" + ProviderUtil.urlEncode(address), headers);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonArray()) {
            return result;
        }
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (item.isJsonObject()) {
                result.add(Normalizer.normalizeEmail(Json.toDict(item), address));
            }
        }
        return result;
    }
}
