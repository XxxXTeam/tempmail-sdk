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
 * Dropmail 渠道 — https://dropmail.me GraphQL API。
 * 需先获取 af_ 令牌，再通过 GraphQL mutation 创建 session。
 */
public final class Dropmail {

    private static final String CHANNEL = "dropmail";
    private static final String TOKEN_GENERATE_URL = "https://dropmail.me/api/token/generate";
    private static final String CREATE_SESSION_QUERY =
            "mutation {introduceSession {id, expiresAt, addresses{id, address}}}";
    private static final String GET_MAILS_QUERY =
            "query ($id: ID!) { session(id:$id) { mails { id, rawSize, fromAddr, toAddr, receivedAt, "
                    + "text, headerFrom, headerSubject, html } } }";

    private static final Object LOCK = new Object();
    private static String cachedToken;
    private static long cachedExpiry; // System.nanoTime() 基准

    private Dropmail() {
    }

    private static Map<String, String> tokenHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json");
        h.put("Content-Type", "application/json");
        h.put("Origin", "https://dropmail.me");
        h.put("Referer", "https://dropmail.me/api/");
        return h;
    }

    /**
     * 获取或刷新 af_ 令牌。
     */
    private static String resolveAfToken() {
        // 优先环境变量
        String envToken = System.getenv("DROPMAIL_AUTH_TOKEN");
        if (envToken != null && !envToken.isBlank()) {
            return envToken.trim();
        }
        String envToken2 = System.getenv("DROPMAIL_API_TOKEN");
        if (envToken2 != null && !envToken2.isBlank()) {
            return envToken2.trim();
        }

        synchronized (LOCK) {
            long now = System.nanoTime();
            if (cachedToken != null && now < cachedExpiry) {
                return cachedToken;
            }
            // 生成新令牌
            Map<String, Object> payload = new LinkedHashMap<>();
            payload.put("type", "af");
            payload.put("lifetime", "1h");
            HttpResult resp = HttpClient.post(TOKEN_GENERATE_URL, Json.serialize(payload),
                    "application/json", tokenHeaders());
            resp.ensureSuccess();
            JsonElement parsed = Json.parse(resp.getBody());
            String tok = Json.str(parsed, "token").trim();
            if (tok.isEmpty() || !tok.startsWith("af_")) {
                String err = Json.str(parsed, "error");
                throw new RuntimeException("dropmail: token/generate 未返回有效 af_ 令牌: " + err);
            }
            cachedToken = tok;
            cachedExpiry = now + 50L * 60 * 1_000_000_000L; // 50分钟
            return tok;
        }
    }

    private static String graphqlUrl() {
        return "https://dropmail.me/api/graphql/" + ProviderUtil.urlEncode(resolveAfToken());
    }

    private static JsonObject graphqlRequest(String query, Map<String, String> variables) {
        Map<String, Object> form = new LinkedHashMap<>();
        form.put("query", query);
        if (variables != null && !variables.isEmpty()) {
            form.put("variables", Json.serialize(variables));
        }
        // 使用 form-urlencoded
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, Object> kv : form.entrySet()) {
            if (sb.length() > 0) sb.append('&');
            sb.append(ProviderUtil.urlEncode(kv.getKey()))
                    .append('=')
                    .append(ProviderUtil.urlEncode(String.valueOf(kv.getValue())));
        }
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Content-Type", "application/x-www-form-urlencoded");
        HttpResult resp = HttpClient.post(graphqlUrl(), sb.toString(),
                "application/x-www-form-urlencoded", h);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("dropmail: GraphQL 响应格式无效");
        }
        JsonObject obj = parsed.getAsJsonObject();
        if (obj.has("errors") && obj.get("errors").isJsonArray() && obj.getAsJsonArray("errors").size() > 0) {
            String msg = Json.str(obj.getAsJsonArray("errors").get(0), "message");
            throw new RuntimeException("dropmail: GraphQL error: " + msg);
        }
        JsonElement dataEl = obj.get("data");
        return dataEl != null && dataEl.isJsonObject() ? dataEl.getAsJsonObject() : new JsonObject();
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        JsonObject data = graphqlRequest(CREATE_SESSION_QUERY, null);
        JsonElement sessionEl = data.get("introduceSession");
        if (sessionEl == null || !sessionEl.isJsonObject()) {
            throw new RuntimeException("dropmail: 创建 session 失败");
        }
        JsonObject session = sessionEl.getAsJsonObject();
        String sessionId = Json.str(session, "id");
        JsonElement addrsEl = session.get("addresses");
        if (sessionId.isEmpty() || addrsEl == null || !addrsEl.isJsonArray()
                || addrsEl.getAsJsonArray().isEmpty()) {
            throw new RuntimeException("dropmail: 响应中缺少 session ID 或地址");
        }
        String address = Json.str(addrsEl.getAsJsonArray().get(0), "address");
        if (address.isEmpty()) {
            throw new RuntimeException("dropmail: 地址为空");
        }
        String expiresAt = Json.str(session, "expiresAt");
        return new EmailInfo(CHANNEL, address, sessionId, null,
                expiresAt.isEmpty() ? null : expiresAt);
    }

    /**
     * 获取邮件列表。
     *
     * @param token     session ID
     * @param email     邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) {
            throw new IllegalArgumentException("dropmail: token 不能为空");
        }
        Map<String, String> vars = new LinkedHashMap<>();
        vars.put("id", token);
        JsonObject data = graphqlRequest(GET_MAILS_QUERY, vars);

        JsonElement sessionEl = data.get("session");
        if (sessionEl == null || !sessionEl.isJsonObject()) return new ArrayList<>();
        JsonElement mailsEl = sessionEl.getAsJsonObject().get("mails");
        if (mailsEl == null || !mailsEl.isJsonArray()) return new ArrayList<>();

        List<Email> result = new ArrayList<>();
        for (JsonElement mailEl : mailsEl.getAsJsonArray()) {
            if (!mailEl.isJsonObject()) continue;
            JsonObject mail = mailEl.getAsJsonObject();
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(mail, "id"));
            raw.put("from", Json.str(mail, "fromAddr"));
            raw.put("to", Json.str(mail, "toAddr").isEmpty() ? email : Json.str(mail, "toAddr"));
            raw.put("subject", Json.str(mail, "headerSubject"));
            raw.put("text", Json.str(mail, "text"));
            raw.put("html", Json.str(mail, "html"));
            raw.put("received_at", Json.str(mail, "receivedAt"));
            result.add(Normalizer.normalizeEmail(raw, email));
        }
        return result;
    }
}
