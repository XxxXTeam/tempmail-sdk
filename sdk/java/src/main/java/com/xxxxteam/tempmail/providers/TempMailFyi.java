package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * temp-mail.fyi 渠道 — https://temp-mail.fyi
 * GET / 获取 PHPSESSID + CSRF，POST /api/generate_email.php 创建，
 * POST /api/get_emails.php 收信。token 使用 base64 编码，前缀 "tmf1:"。
 */
public final class TempMailFyi {

    private static final String BASE_URL = "https://temp-mail.fyi";
    private static final String TOK_PREFIX = "tmf1:";
    private static final String UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private TempMailFyi() {
    }

    private static Map<String, String> apiHeaders(String csrf, String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7");
        h.put("Content-Type", "application/json");
        h.put("X-CSRF-Token", csrf);
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Referer", BASE_URL + "/");
        h.put("Cookie", cookie);
        return h;
    }

    private static String encodeToken(String csrf, String cookie) {
        String raw = "{\"t\":\"" + csrf + "\",\"c\":\"" + cookie.replace("\"", "\\\"") + "\"}";
        return TOK_PREFIX + ProviderUtil.base64Encode(raw.getBytes(StandardCharsets.UTF_8));
    }

    private static String[] decodeToken(String token) {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw new RuntimeException("tempmail-fyi: 无效的 token");
        }
        byte[] decoded = ProviderUtil.base64Decode(token.substring(TOK_PREFIX.length()));
        JsonElement obj = Json.parse(new String(decoded, StandardCharsets.UTF_8));
        String csrf = Json.str(obj, "t").trim();
        String cookie = Json.str(obj, "c").trim();
        if (csrf.isEmpty()) {
            throw new RuntimeException("tempmail-fyi: token 中缺少 CSRF");
        }
        return new String[]{csrf, cookie};
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> pageH = new LinkedHashMap<>();
        pageH.put("User-Agent", UA);
        pageH.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        HttpResult resp = HttpClient.get(BASE_URL + "/", pageH);
        resp.ensureSuccess();
        String cookie = ProviderUtil.extractAllCookies(resp);
        // 提取 CSRF: csrfToken" value="xxx"
        String csrf = ProviderUtil.regexExtract(resp.getBody(), "csrfToken\"\\s*value=\"([^\"]+)\"");
        if (csrf.isEmpty()) {
            throw new RuntimeException("tempmail-fyi: 未能从首页提取 CSRF token");
        }
        // POST /api/generate_email.php
        HttpResult resp2 = HttpClient.post(BASE_URL + "/api/generate_email.php", "{}",
                "application/json", apiHeaders(csrf, cookie));
        resp2.ensureSuccess();
        JsonElement data = Json.parse(resp2.getBody());
        JsonElement successEl = data.getAsJsonObject().get("success");
        if (successEl == null || !successEl.getAsBoolean()) {
            throw new RuntimeException("tempmail-fyi: 创建邮箱失败");
        }
        String address = Json.str(data, "email_address").trim();
        if (address.isEmpty() || !address.contains("@")) {
            throw new RuntimeException("tempmail-fyi: 邮箱地址无效");
        }
        return new EmailInfo("tempmail-fyi", address, encodeToken(csrf, cookie), null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token base64 编码的认证信息
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("tempmail-fyi: 邮箱地址为空");
        }
        String[] parts = decodeToken(token);
        String csrf = parts[0];
        String cookie = parts[1];
        String body = "{\"email_address\":\"" + address + "\"}";
        HttpResult resp = HttpClient.post(BASE_URL + "/api/get_emails.php", body,
                "application/json", apiHeaders(csrf, cookie));
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonElement successEl = data.getAsJsonObject().get("success");
        if (successEl == null || !successEl.getAsBoolean()) {
            return new ArrayList<>();
        }
        JsonArray emails = Json.arr(data, "emails");
        if (emails == null || emails.size() == 0) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : emails) {
            if (item == null || !item.isJsonObject()) continue;
            result.add(Normalizer.normalizeEmail(Json.toDict(item), address));
        }
        return result;
    }
}
