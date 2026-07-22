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
 * Byom 渠道（byom.de）。
 * 纯 GET 无认证，直接生成随机用户名。
 */
public final class Byom {

    private static final String DOMAIN = "byom.de";
    private static final String BASE = "https://api.byom.de";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json, text/plain, */*");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    }

    private Byom() {
    }

    /**
     * 生成随机邮箱地址，无需 API 调用。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String username = ProviderUtil.randomString(10);
        return new EmailInfo("byom", username + "@" + DOMAIN);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("byom: empty email");
        }
        String e = email.trim();
        String username = e.split("@")[0];
        if (username.isEmpty()) {
            throw new RuntimeException("byom: invalid email format");
        }
        HttpResult resp = HttpClient.get(BASE + "/mails/" + ProviderUtil.urlEncode(username), HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonArray()) {
            return result;
        }
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (item.isJsonObject()) {
                result.add(Normalizer.normalizeEmail(Json.toDict(item), e));
            }
        }
        return result;
    }
}
