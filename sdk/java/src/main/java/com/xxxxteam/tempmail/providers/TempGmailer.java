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
 * TempGmailer 渠道 — https://tempgmailer.com
 * GET 首页获取 CSRF + Laravel session，POST /get-gmail 创建，POST /get-inbox 收信。
 */
public final class TempGmailer {

    private static final String BASE_URL = "https://tempgmailer.com";
    private static final String UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private TempGmailer() {
    }

    private static Map<String, String> apiHeaders(String csrf, String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "application/json, text/plain, */*");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        h.put("Referer", BASE_URL + "/");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("X-TempGmailer-Auth", "frontend");
        h.put("Content-Type", "application/json");
        h.put("X-CSRF-TOKEN", csrf);
        h.put("Cookie", cookie);
        return h;
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 JSON {"csrfToken":"...","cookies":"..."}
     */
    public static EmailInfo generate() {
        // 获取首页 CSRF + session cookie
        Map<String, String> pageH = new LinkedHashMap<>();
        pageH.put("User-Agent", UA);
        pageH.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        HttpResult resp = HttpClient.get(BASE_URL, pageH);
        resp.ensureSuccess();
        String csrf = ProviderUtil.extractCsrfToken(resp.getBody());
        if (csrf.isEmpty()) {
            throw new RuntimeException("tempgmailer: 无法提取 CSRF token");
        }
        String cookie = ProviderUtil.extractAllCookies(resp);
        if (cookie.isEmpty()) {
            throw new RuntimeException("tempgmailer: 未获取到 session cookie");
        }
        // POST /get-gmail
        String body = "{\"refresh\":true,\"adblock\":0}";
        HttpResult resp2 = HttpClient.post(BASE_URL + "/get-gmail", body, "application/json",
                apiHeaders(csrf, cookie));
        resp2.ensureSuccess();
        // 合并 cookie
        String newCk = ProviderUtil.extractAllCookies(resp2);
        if (!newCk.isEmpty()) {
            cookie = ProviderUtil.mergeCookieStrings(cookie, newCk);
        }
        JsonElement data = Json.parse(resp2.getBody());
        boolean success = false;
        JsonElement successEl = data.getAsJsonObject().get("success");
        if (successEl != null && successEl.getAsBoolean()) success = true;
        if (!success) {
            throw new RuntimeException("tempgmailer: 创建邮箱失败");
        }
        // data.data.email
        JsonElement dataObj = data.getAsJsonObject().get("data");
        String address = "";
        if (dataObj != null && dataObj.isJsonObject()) {
            address = Json.str(dataObj, "email").trim();
        }
        if (address.isEmpty() || !address.contains("@")) {
            throw new RuntimeException("tempgmailer: 未返回邮箱地址");
        }
        String token = "{\"csrfToken\":\"" + csrf + "\",\"cookies\":\"" + cookie.replace("\"", "\\\"") + "\"}";
        return new EmailInfo("tempgmailer", address, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 格式认证信息
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("tempgmailer: 邮箱地址为空");
        }
        JsonElement tokenData = Json.parse(token != null ? token : "{}");
        String csrf = Json.str(tokenData, "csrfToken").trim();
        String cookie = Json.str(tokenData, "cookies").trim();
        if (csrf.isEmpty() || cookie.isEmpty()) {
            throw new RuntimeException("tempgmailer: token 无效");
        }
        String body = "{\"email\":\"" + address + "\",\"adblock\":0}";
        HttpResult resp = HttpClient.post(BASE_URL + "/get-inbox", body, "application/json",
                apiHeaders(csrf, cookie));
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonElement successEl = data.getAsJsonObject().get("success");
        if (successEl == null || !successEl.getAsBoolean()) {
            return new ArrayList<>();
        }
        JsonElement dataObj = data.getAsJsonObject().get("data");
        if (dataObj == null || !dataObj.isJsonObject()) {
            return new ArrayList<>();
        }
        JsonArray messages = Json.arr(dataObj, "messages");
        if (messages == null || messages.size() == 0) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : messages) {
            if (item == null || !item.isJsonObject()) continue;
            // from 可能是对象或字符串
            String fromField = "";
            JsonElement fromEl = item.getAsJsonObject().get("from");
            if (fromEl != null) {
                if (fromEl.isJsonObject()) {
                    fromField = Json.str(fromEl, "address");
                } else if (fromEl.isJsonPrimitive()) {
                    fromField = fromEl.getAsString();
                }
            }
            String htmlContent = ProviderUtil.firstNonEmpty(Json.str(item, "body"), Json.str(item, "intro"));
            String textContent = Json.str(item, "intro");
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(item, "id"));
            raw.put("from", fromField);
            raw.put("to", address);
            raw.put("subject", Json.str(item, "subject"));
            raw.put("text", textContent);
            raw.put("html", htmlContent);
            raw.put("date", Json.str(item, "createdAt"));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }
}
