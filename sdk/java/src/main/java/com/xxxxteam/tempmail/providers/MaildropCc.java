package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
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
 * maildrop.cc 渠道 — GraphQL API，无认证。
 * 公共邮箱服务，随机生成用户名 + "@maildrop.cc"。
 */
public final class MaildropCc {

    private static final String CHANNEL = "maildrop-cc";
    private static final String DOMAIN = "maildrop.cc";
    private static final String GRAPHQL_URL = "https://api.maildrop.cc/graphql";

    private MaildropCc() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json");
        h.put("Content-Type", "application/json");
        h.put("Origin", "https://maildrop.cc");
        h.put("Referer", "https://maildrop.cc/");
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        return h;
    }

    private static JsonObject doGraphql(String query) {
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("query", query);
        HttpResult resp = HttpClient.post(GRAPHQL_URL, Json.serialize(body), "application/json", headers());
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            return new JsonObject();
        }
        return parsed.getAsJsonObject();
    }

    private static String mailbox(String email) {
        if (email == null || email.isBlank()) return "";
        int at = email.indexOf('@');
        return at >= 0 ? email.substring(0, at) : email;
    }

    /**
     * 创建 maildrop.cc 临时邮箱（无需 API 调用）。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String email = ProviderUtil.randomString(10) + "@" + DOMAIN;
        return new EmailInfo(CHANNEL, email, email, null, null);
    }

    /**
     * 获取邮件列表（先查 inbox 列表，再逐封查详情）。
     *
     * @param email 邮箱地址
     * @param token token（本渠道不使用）
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String mb = mailbox(email);
        if (mb.isEmpty()) {
            throw new IllegalArgumentException("maildrop-cc: 邮箱地址为空");
        }

        // 查询邮件列表
        String mbJson = Json.serialize(mb);
        String inboxQuery = "query { inbox(mailbox: " + mbJson + ") { id headerfrom subject date } }";
        JsonObject inboxResp = doGraphql(inboxQuery);

        JsonElement dataEl = inboxResp.get("data");
        if (dataEl == null || !dataEl.isJsonObject()) return new ArrayList<>();
        JsonElement inboxEl = dataEl.getAsJsonObject().get("inbox");
        if (inboxEl == null || !inboxEl.isJsonArray()) return new ArrayList<>();

        List<Email> result = new ArrayList<>();
        for (JsonElement item : inboxEl.getAsJsonArray()) {
            if (!item.isJsonObject()) continue;
            JsonObject obj = item.getAsJsonObject();
            String mid = Json.str(obj, "id");

            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", mid);
            raw.put("from", Json.str(obj, "headerfrom"));
            raw.put("subject", Json.str(obj, "subject"));
            raw.put("date", Json.str(obj, "date"));

            // 查询单封详情补全 html 正文
            if (!mid.isEmpty()) {
                try {
                    String midJson = Json.serialize(mid);
                    String msgQuery = "query { message(mailbox: " + mbJson + ", id: " + midJson
                            + ") { id headerfrom subject date html } }";
                    JsonObject msgResp = doGraphql(msgQuery);
                    JsonElement msgData = msgResp.get("data");
                    if (msgData != null && msgData.isJsonObject()) {
                        JsonElement msgEl = msgData.getAsJsonObject().get("message");
                        if (msgEl != null && msgEl.isJsonObject()) {
                            JsonObject msg = msgEl.getAsJsonObject();
                            raw.put("id", Json.str(msg, "id").isEmpty() ? mid : Json.str(msg, "id"));
                            raw.put("from", Json.str(msg, "headerfrom"));
                            raw.put("subject", Json.str(msg, "subject"));
                            raw.put("date", Json.str(msg, "date"));
                            raw.put("html", Json.str(msg, "html"));
                        }
                    }
                } catch (RuntimeException ignored) {
                }
            }
            result.add(Normalizer.normalizeEmail(raw, email));
        }
        return result;
    }
}
