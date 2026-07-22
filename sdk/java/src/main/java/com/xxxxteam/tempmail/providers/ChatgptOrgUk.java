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
import java.util.concurrent.ThreadLocalRandom;

/**
 * mail.chatgpt.org.uk 渠道 — https://mail.chatgpt.org.uk
 *
 * <p>GET /api/domains/public 获取域名 → POST /api/inbox-token 创建收件箱
 * → GET /api/emails?email=xxx 获取邮件。</p>
 */
public final class ChatgptOrgUk {

    private static final String BASE_URL = "https://mail.chatgpt.org.uk/api";

    private ChatgptOrgUk() {
    }

    private static Map<String, String> defaultHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "*/*");
        h.put("Referer", "https://mail.chatgpt.org.uk/zh/");
        h.put("Origin", "https://mail.chatgpt.org.uk");
        return h;
    }

    /**
     * 创建 chatgpt-org-uk 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        // 获取域名
        HttpResult domResp = HttpClient.get(BASE_URL + "/domains/public", defaultHeaders());
        domResp.ensureSuccess();
        JsonObject domBody = Json.parseObject(domResp.getBody());
        if (domBody == null || !domBody.has("success")) {
            throw new RuntimeException("chatgpt-org-uk: 获取域名失败");
        }
        JsonObject domData = domBody.has("data") && domBody.get("data").isJsonObject()
                ? domBody.getAsJsonObject("data") : null;
        JsonArray domains = domData != null ? Json.arr(domData, "domains") : null;
        if (domains == null || domains.isEmpty()) {
            throw new RuntimeException("chatgpt-org-uk: 无可用域名");
        }

        List<String> activeDomains = new ArrayList<>();
        for (JsonElement d : domains) {
            if (d.isJsonObject()) {
                JsonObject dObj = d.getAsJsonObject();
                if (dObj.has("is_active") && dObj.get("is_active").getAsInt() == 1) {
                    activeDomains.add(Json.str(dObj, "domain_name"));
                }
            }
        }
        if (activeDomains.isEmpty()) throw new RuntimeException("chatgpt-org-uk: 无可用域名");

        String domain = activeDomains.get(ThreadLocalRandom.current().nextInt(activeDomains.size()));
        String username = ProviderUtil.randomString(10);
        String email = username + "@" + domain;

        // 创建收件箱
        Map<String, String> h = defaultHeaders();
        h.put("Content-Type", "application/json");
        String payload = Json.serialize(Map.of("email", email));
        HttpResult tokResp = HttpClient.post(BASE_URL + "/inbox-token", payload, "application/json", h);
        tokResp.ensureSuccess();

        // 提取 gm_sid cookie
        String gmSid = "";
        for (String c : tokResp.getSetCookies()) {
            int idx = c.indexOf("gm_sid=");
            if (idx >= 0) {
                String seg = c.substring(idx + 7);
                int semi = seg.indexOf(';');
                gmSid = semi < 0 ? seg : seg.substring(0, semi);
                break;
            }
        }

        JsonObject tokBody = Json.parseObject(tokResp.getBody());
        JsonObject auth = tokBody != null && tokBody.has("auth") && tokBody.get("auth").isJsonObject()
                ? tokBody.getAsJsonObject("auth") : null;
        String inboxToken = auth != null ? Json.str(auth, "token") : "";
        if (inboxToken.isEmpty()) throw new RuntimeException("chatgpt-org-uk: 响应缺少 token");

        String packed = Json.serialize(Map.of("gmSid", gmSid, "inbox", inboxToken));
        return new EmailInfo("chatgpt-org-uk", email, packed, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 编码的 {gmSid, inbox}
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) {
            throw new RuntimeException("chatgpt-org-uk: token 不能为空");
        }

        JsonObject tok = Json.parseObject(token);
        String gmSid = tok != null ? Json.str(tok, "gmSid") : "";
        String inbox = tok != null ? Json.str(tok, "inbox") : token;

        // 如果 gmSid 为空，重新创建 inbox
        if (gmSid.isEmpty()) {
            Map<String, String> h2 = defaultHeaders();
            h2.put("Content-Type", "application/json");
            HttpResult r2 = HttpClient.post(BASE_URL + "/inbox-token",
                    Json.serialize(Map.of("email", email)), "application/json", h2);
            r2.ensureSuccess();
            JsonObject b2 = Json.parseObject(r2.getBody());
            JsonObject a2 = b2 != null && b2.has("auth") && b2.get("auth").isJsonObject()
                    ? b2.getAsJsonObject("auth") : null;
            inbox = a2 != null ? Json.str(a2, "token") : "";
            for (String c : r2.getSetCookies()) {
                int idx = c.indexOf("gm_sid=");
                if (idx >= 0) {
                    String seg = c.substring(idx + 7);
                    int semi = seg.indexOf(';');
                    gmSid = semi < 0 ? seg : seg.substring(0, semi);
                    break;
                }
            }
        }

        Map<String, String> h = defaultHeaders();
        h.put("x-inbox-token", inbox);
        if (!gmSid.isEmpty()) h.put("Cookie", "gm_sid=" + gmSid);

        HttpResult resp = HttpClient.get(
                BASE_URL + "/emails?email=" + ProviderUtil.urlEncode(email), h);
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null || !body.has("success")) return new ArrayList<>();

        JsonObject data = body.has("data") && body.get("data").isJsonObject()
                ? body.getAsJsonObject("data") : null;
        JsonArray emails = data != null ? Json.arr(data, "emails") : null;
        if (emails == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : emails) {
            if (item.isJsonObject()) {
                out.add(Normalizer.normalizeEmail(Json.toDict(item), email));
            }
        }
        return out;
    }
}
