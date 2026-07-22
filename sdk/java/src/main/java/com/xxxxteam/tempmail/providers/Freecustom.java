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
 * Freecustom 渠道（freecustom.email）。
 * 免注册临时邮箱，任意 local part @ 可用域名即时可收信。
 * 读信时动态获取匿名 JWT（有效期约 2 小时）。
 */
public final class Freecustom {

    private static final String SITE_URL = "https://www.freecustom.email";
    private static final String DOMAINS_URL = "https://api2.freecustom.email/domains";
    private static final String REFERER = "https://www.freecustom.email/en";

    private Freecustom() {
    }

    private static Map<String, String> baseHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
        h.put("Accept", "application/json");
        h.put("Referer", REFERER);
        return h;
    }

    /**
     * 挑选一个当前可收信的域名。
     */
    private static String pickDomain() {
        Map<String, String> h = baseHeaders();
        HttpResult resp = HttpClient.get(DOMAINS_URL, h);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("freecustom: 域名列表为空");
        }
        JsonElement lstEl = parsed.getAsJsonObject().get("data");
        if (lstEl == null || !lstEl.isJsonArray()) {
            throw new RuntimeException("freecustom: 域名列表为空");
        }
        List<String> usable = new ArrayList<>();
        List<String> all = new ArrayList<>();
        for (JsonElement item : lstEl.getAsJsonArray()) {
            if (!item.isJsonObject()) {
                continue;
            }
            String domain = Json.str(item, "domain").trim();
            if (domain.isEmpty()) {
                continue;
            }
            all.add(domain);
            String tier = Json.str(item, "tier").trim();
            boolean expiringSoon = item.getAsJsonObject().has("expiring_soon")
                    && item.getAsJsonObject().get("expiring_soon").isJsonPrimitive()
                    && item.getAsJsonObject().get("expiring_soon").getAsBoolean();
            if ("free".equals(tier) && !expiringSoon) {
                usable.add(domain);
            }
        }
        List<String> pool = usable.isEmpty() ? all : usable;
        if (pool.isEmpty()) {
            throw new RuntimeException("freecustom: 无可用域名");
        }
        return pool.get(java.util.concurrent.ThreadLocalRandom.current().nextInt(pool.size()));
    }

    /**
     * 获取匿名访问令牌（JWT）。
     */
    private static String fetchAuthToken() {
        Map<String, String> h = baseHeaders();
        h.put("Content-Type", "application/json");
        HttpResult resp = HttpClient.post(SITE_URL + "/api/auth", null, "application/json", h);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("freecustom: 令牌响应无效");
        }
        String token = Json.str(parsed, "token").trim();
        if (token.isEmpty()) {
            throw new RuntimeException("freecustom: 令牌响应无效");
        }
        return token;
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String domain = pickDomain();
        String email = ProviderUtil.randomString(10) + "@" + domain;
        return new EmailInfo("freecustom", email, email, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token token（即邮箱地址）
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String addr = (token != null && !token.isBlank()) ? token.trim()
                : (email != null ? email.trim() : "");
        if (addr.isEmpty()) {
            throw new RuntimeException("freecustom: 缺少邮箱地址");
        }
        String jwt = fetchAuthToken();
        Map<String, String> authHeaders = baseHeaders();
        authHeaders.put("Authorization", "Bearer " + jwt);
        authHeaders.put("x-fce-client", "web-client");

        HttpResult listResp = HttpClient.get(
                SITE_URL + "/api/public-mailbox?fullMailboxId=" + ProviderUtil.urlEncode(addr), authHeaders);
        listResp.ensureSuccess();
        JsonElement listParsed = Json.parse(listResp.getBody());
        List<Email> result = new ArrayList<>();
        if (listParsed == null || !listParsed.isJsonObject()) {
            return result;
        }
        JsonObject listObj = listParsed.getAsJsonObject();
        if (!listObj.has("success") || !listObj.get("success").getAsBoolean()) {
            return result;
        }
        JsonElement itemsEl = listObj.get("data");
        if (itemsEl == null || !itemsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement itemEl : itemsEl.getAsJsonArray()) {
            if (!itemEl.isJsonObject()) {
                continue;
            }
            String msgId = Json.str(itemEl, "id").trim();
            if (msgId.isEmpty()) {
                continue;
            }
            Map<String, Object> full = Json.toDict(itemEl);
            // 尝试补全正文
            try {
                HttpResult msgResp = HttpClient.get(
                        SITE_URL + "/api/public-mailbox?fullMailboxId=" + ProviderUtil.urlEncode(addr)
                                + "&messageId=" + ProviderUtil.urlEncode(msgId), authHeaders);
                if (msgResp.getStatusCode() >= 200 && msgResp.getStatusCode() < 300) {
                    JsonElement msgParsed = Json.parse(msgResp.getBody());
                    if (msgParsed != null && msgParsed.isJsonObject()) {
                        JsonObject msgObj = msgParsed.getAsJsonObject();
                        if (msgObj.has("success") && msgObj.get("success").getAsBoolean()
                                && msgObj.has("data") && msgObj.get("data").isJsonObject()) {
                            full = Json.toDict(msgObj.get("data"));
                        }
                    }
                }
            } catch (RuntimeException ignored) {
            }
            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", strVal(full, "id"));
            row.put("from", strVal(full, "from"));
            row.put("to", ProviderUtil.firstNonEmpty(strVal(full, "to"), addr));
            row.put("subject", strVal(full, "subject"));
            row.put("text", strVal(full, "text"));
            row.put("html", strVal(full, "html"));
            row.put("date", strVal(full, "date"));
            result.add(Normalizer.normalizeEmail(row, addr));
        }
        return result;
    }

    private static String strVal(Map<String, Object> m, String key) {
        Object v = m.get(key);
        return v != null ? v.toString() : "";
    }
}
