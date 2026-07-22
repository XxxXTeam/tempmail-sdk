package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
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
 * Temp-Mail.org 渠道 — https://temp-mail.org
 * POST /mailbox 创建（返回 JWT），GET /messages 收信（Bearer 认证）。
 */
public final class TempMailOrg {

    private static final String BASE_URL = "https://web2.temp-mail.org";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36");
        HEADERS.put("Origin", "https://temp-mail.org");
        HEADERS.put("Referer", "https://temp-mail.org/");
    }

    private TempMailOrg() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 JWT
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.post(BASE_URL + "/mailbox", null, null, HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String token = Json.str(data, "token").trim();
        String mailbox = Json.str(data, "mailbox").trim();
        if (token.isEmpty() || mailbox.isEmpty()) {
            throw new RuntimeException("temp-mail-org: 创建邮箱失败");
        }
        return new EmailInfo("temp-mail-org", mailbox, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JWT 令牌
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String jwt = (token != null ? token : "").trim();
        String address = (email != null ? email : "").trim();
        if (jwt.isEmpty()) {
            throw new RuntimeException("temp-mail-org: token 为空");
        }
        if (address.isEmpty()) {
            throw new RuntimeException("temp-mail-org: 邮箱地址为空");
        }
        Map<String, String> h = new LinkedHashMap<>(HEADERS);
        h.put("Authorization", "Bearer " + jwt);
        HttpResult resp = HttpClient.get(BASE_URL + "/messages", h);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonArray messages = Json.arr(data, "messages");
        if (messages == null) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : messages) {
            if (item == null || !item.isJsonObject()) {
                continue;
            }
            String msgId = Json.str(item, "_id").trim();
            if (msgId.isEmpty()) {
                continue;
            }
            JsonElement detail = fetchDetail(jwt, msgId);
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", msgId);
            if (detail != null) {
                raw.put("from", Json.str(detail, "from"));
                raw.put("subject", Json.str(detail, "subject"));
                raw.put("text", Json.str(detail, "bodyPreview"));
                raw.put("html", Json.str(detail, "bodyHtml"));
                raw.put("date", Json.str(detail, "createdAt"));
            } else {
                raw.put("from", Json.str(item, "from"));
                raw.put("subject", Json.str(item, "subject"));
                raw.put("text", Json.str(item, "bodyPreview"));
                raw.put("date", String.valueOf(Json.str(item, "receivedAt")));
            }
            raw.put("to", address);
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static JsonElement fetchDetail(String jwt, String msgId) {
        try {
            Map<String, String> h = new LinkedHashMap<>(HEADERS);
            h.put("Authorization", "Bearer " + jwt);
            HttpResult resp = HttpClient.get(BASE_URL + "/messages/" + ProviderUtil.urlEncode(msgId), h);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parse(resp.getBody());
        } catch (Exception e) {
            return null;
        }
    }
}
