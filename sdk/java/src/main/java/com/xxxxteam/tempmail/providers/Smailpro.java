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
 * smailpro.com 渠道实现。
 *
 * <p>两段式流程：先从 smailpro.com/app/payload 获取 JWT，再携带 JWT 调用 api.sonjj.com 接口。</p>
 */
public final class Smailpro {

    private static final String BASE_URL = "https://smailpro.com";
    private static final String API_BASE_URL = "https://api.sonjj.com/v1/temp_email";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        HEADERS.put("Accept", "application/json, text/plain, */*");
        HEADERS.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        HEADERS.put("Referer", BASE_URL + "/");
    }

    private Smailpro() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        JsonObject data = callApi(API_BASE_URL + "/create", null);
        String email = Json.str(data, "email").trim();
        if (email.isEmpty()) {
            throw new RuntimeException("smailpro: 创建邮箱失败，未返回邮箱地址");
        }
        String expiresAt = Json.str(data, "expired_at");
        return new EmailInfo("smailpro", email, email,
                expiresAt.isEmpty() ? null : parseLong(expiresAt), null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 令牌（本渠道不参与逻辑）
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String addr = (email != null ? email : "").trim();
        if (addr.isEmpty()) {
            throw new RuntimeException("smailpro: 邮箱地址为空");
        }
        Map<String, String> extra = new LinkedHashMap<>();
        extra.put("email", addr);
        JsonObject data = callApi(API_BASE_URL + "/inbox", extra);

        // API 响应结构: {"status": true, "data": {"messages": [...]}}
        JsonArray messages = null;
        JsonElement inner = data.get("data");
        if (inner != null && inner.isJsonObject()) {
            messages = Json.arr(inner, "messages");
        }
        if (messages == null) {
            messages = Json.arr(data, "messages");
        }
        List<Email> result = new ArrayList<>();
        if (messages == null || messages.isEmpty()) {
            return result;
        }
        for (JsonElement item : messages) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject m = item.getAsJsonObject();
            String mid = Json.str(m, "mid").trim();
            String[] bodies = fetchMessageBody(addr, mid);
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", mid);
            raw.put("from", Json.str(m, "from"));
            raw.put("to", addr);
            raw.put("subject", Json.str(m, "subject"));
            raw.put("date", Json.str(m, "datetime"));
            if (!bodies[0].isEmpty() || !bodies[1].isEmpty()) {
                raw.put("html", bodies[0]);
                raw.put("text", bodies[1]);
            }
            result.add(Normalizer.normalizeEmail(raw, addr));
        }
        return result;
    }

    private static String fetchPayload(String targetUrl, Map<String, String> extra) {
        StringBuilder sb = new StringBuilder(BASE_URL + "/app/payload?url=");
        sb.append(ProviderUtil.urlEncode(targetUrl));
        if (extra != null) {
            for (Map.Entry<String, String> kv : extra.entrySet()) {
                sb.append('&').append(ProviderUtil.urlEncode(kv.getKey()))
                        .append('=').append(ProviderUtil.urlEncode(kv.getValue()));
            }
        }
        HttpResult resp = HttpClient.get(sb.toString(), HEADERS);
        resp.ensureSuccess();
        String payload = resp.getBody().trim().replace("\"", "");
        if (payload.isEmpty()) {
            throw new RuntimeException("smailpro: payload 为空");
        }
        return payload;
    }

    private static JsonObject callApi(String targetUrl, Map<String, String> extra) {
        String payload = fetchPayload(targetUrl, extra);
        HttpResult resp = HttpClient.get(
                targetUrl + "?payload=" + ProviderUtil.urlEncode(payload), HEADERS);
        resp.ensureSuccess();
        JsonObject obj = Json.parseObject(resp.getBody());
        if (obj == null) {
            throw new RuntimeException("smailpro: 无效的 API 响应");
        }
        return obj;
    }

    private static String[] fetchMessageBody(String email, String mid) {
        if (mid.isEmpty()) {
            return new String[]{"", ""};
        }
        try {
            Map<String, String> extra = new LinkedHashMap<>();
            extra.put("email", email);
            extra.put("mid", mid);
            JsonObject data = callApi(API_BASE_URL + "/message", extra);
            String html = Json.str(data, "body");
            String text = Json.str(data, "textBody");
            return new String[]{html, text};
        } catch (RuntimeException e) {
            return new String[]{"", ""};
        }
    }

    private static Long parseLong(String s) {
        try {
            return Long.parseLong(s);
        } catch (NumberFormatException e) {
            return null;
        }
    }
}
