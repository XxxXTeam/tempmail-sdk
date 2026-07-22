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
 * Inboxes 渠道（inboxes.com）。
 */
public final class Inboxes {

    private static final String API_BASE = "https://inboxes.com/api/v2";
    private static final String DEFAULT_DOMAIN = "blondmail.com";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("Origin", "https://inboxes.com");
        HEADERS.put("Referer", "https://inboxes.com/");
        HEADERS.put("User-Agent", "Mozilla/5.0");
    }

    private Inboxes() {
    }

    /**
     * 获取可用域名列表。
     */
    private static List<String> getDomains() {
        HttpResult resp = HttpClient.get(API_BASE + "/domain", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("inboxes: no domains");
        }
        JsonElement domainsEl = parsed.getAsJsonObject().get("domains");
        if (domainsEl == null || !domainsEl.isJsonArray()) {
            throw new RuntimeException("inboxes: no domains");
        }
        List<String> domains = new ArrayList<>();
        for (JsonElement item : domainsEl.getAsJsonArray()) {
            if (item.isJsonObject()) {
                String qdn = Json.str(item, "qdn").trim();
                if (!qdn.isEmpty()) {
                    domains.add(qdn);
                }
            }
        }
        if (domains.isEmpty()) {
            throw new RuntimeException("inboxes: no domains");
        }
        return domains;
    }

    private static String pickDomain(List<String> domains, String preferred) {
        String wanted = (preferred != null) ? preferred.trim().replaceFirst("^@", "").toLowerCase() : "";
        if (!wanted.isEmpty()) {
            for (String d : domains) {
                if (d.toLowerCase().equals(wanted)) {
                    return d;
                }
            }
        }
        if (domains.contains(DEFAULT_DOMAIN)) {
            return DEFAULT_DOMAIN;
        }
        return domains.get(0);
    }

    /**
     * 创建临时邮箱（支持指定域名）。
     *
     * @param domain 域名，可为 null
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        List<String> domains = getDomains();
        String selected = pickDomain(domains, domain);
        return new EmailInfo("inboxes", ProviderUtil.randomLocalSdk() + "@" + selected);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new RuntimeException("inboxes: empty email");
        }
        String address = email.trim();
        HttpResult resp = HttpClient.get(
                API_BASE + "/inbox/" + ProviderUtil.urlEncode(address), HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonObject()) {
            return result;
        }
        JsonElement rowsEl = parsed.getAsJsonObject().get("msgs");
        if (rowsEl == null || !rowsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement item : rowsEl.getAsJsonArray()) {
            if (!item.isJsonObject()) {
                continue;
            }
            Map<String, Object> row = Json.toDict(item);
            String uid = row.get("uid") != null ? row.get("uid").toString().trim() : "";
            Map<String, Object> detail = uid.isEmpty() ? null : fetchDetail(uid);
            Map<String, Object> src = detail != null ? detail : row;
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", src.getOrDefault("uid", ""));
            flat.put("from", ProviderUtil.firstNonEmpty(strVal(src, "sf"), strVal(src, "f")));
            flat.put("to", ProviderUtil.firstNonEmpty(strVal(src, "ib"), address));
            flat.put("subject", src.getOrDefault("s", ""));
            flat.put("text", src.getOrDefault("text", ""));
            flat.put("html", src.getOrDefault("html", ""));
            flat.put("date", src.getOrDefault("cr", ""));
            result.add(Normalizer.normalizeEmail(flat, address));
        }
        return result;
    }

    private static Map<String, Object> fetchDetail(String uid) {
        try {
            HttpResult resp = HttpClient.get(
                    API_BASE + "/message/" + ProviderUtil.urlEncode(uid), HEADERS);
            if (resp.getStatusCode() < 200 || resp.getStatusCode() >= 300) {
                return null;
            }
            JsonElement parsed = Json.parse(resp.getBody());
            if (parsed != null && parsed.isJsonObject()) {
                return Json.toDict(parsed);
            }
        } catch (RuntimeException ignored) {
        }
        return null;
    }

    private static String strVal(Map<String, Object> m, String key) {
        Object v = m.get(key);
        return v != null ? v.toString() : "";
    }
}
