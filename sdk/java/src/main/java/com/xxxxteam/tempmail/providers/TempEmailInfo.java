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
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * tempemail.info 渠道 — https://tempemail.info
 * GET / 获取 PHPSESSID + HTML 中 base64 编码的邮箱地址，
 * POST /template/checker.php 获取邮件列表，GET /view/{date} 获取正文。
 * token 为 session cookie 字符串。
 */
public final class TempEmailInfo {

    private static final String BASE_URL = "https://tempemail.info";
    private static final String UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private TempEmailInfo() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 session cookie 字符串
     */
    public static EmailInfo generate() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        h.put("Referer", BASE_URL + "/");
        HttpResult resp = HttpClient.get(BASE_URL + "/", h);
        resp.ensureSuccess();
        String cookie = ProviderUtil.extractAllCookies(resp);
        if (cookie.isEmpty()) {
            throw new RuntimeException("tempemail-info: 未获取到会话 Cookie");
        }
        // 从 HTML 中提取 var emailEncoded = "base64..."
        String encoded = ProviderUtil.regexExtract(resp.getBody(),
                "var\\s+emailEncoded\\s*=\\s*\"([^\"]+)\"");
        if (encoded.isEmpty()) {
            throw new RuntimeException("tempemail-info: 未在页面中找到 emailEncoded");
        }
        String address;
        try {
            address = new String(Base64.getDecoder().decode(encoded), StandardCharsets.UTF_8).trim();
        } catch (Exception e) {
            throw new RuntimeException("tempemail-info: 邮箱地址 base64 解码失败");
        }
        if (address.isEmpty() || !address.contains("@")) {
            throw new RuntimeException("tempemail-info: 解码出的邮箱地址无效");
        }
        return new EmailInfo("tempemail-info", address, cookie, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token session cookie 字符串
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        String cookie = (token != null ? token : "").trim();
        if (cookie.isEmpty()) {
            throw new RuntimeException("tempemail-info: 缺少会话 Cookie");
        }
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Content-Type", "application/x-www-form-urlencoded");
        h.put("Cookie", cookie);
        h.put("Referer", BASE_URL + "/");
        h.put("Origin", BASE_URL);
        // POST /template/checker.php
        HttpResult resp = HttpClient.post(BASE_URL + "/template/checker.php",
                "last_id=0", "application/x-www-form-urlencoded", h);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonArray()) {
            return new ArrayList<>();
        }
        JsonArray rows = parsed.getAsJsonArray();
        if (rows.size() == 0) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement row : rows) {
            if (row == null || !row.isJsonObject()) continue;
            String fromAddr = Json.str(row, "from");
            String name = Json.str(row, "name");
            if (!name.isEmpty() && !name.equals(fromAddr)) {
                fromAddr = name + " <" + fromAddr + ">";
            }
            String date = Json.str(row, "date");
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(row, "id"));
            raw.put("from", fromAddr);
            raw.put("to", address);
            raw.put("subject", Json.str(row, "subject"));
            raw.put("date", date);
            raw.put("html", fetchBody(cookie, date));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static String fetchBody(String cookie, String date) {
        if (date == null || date.trim().isEmpty()) return "";
        try {
            Map<String, String> h = new LinkedHashMap<>();
            h.put("User-Agent", UA);
            h.put("Accept", "text/html,*/*");
            h.put("Cookie", cookie);
            h.put("Referer", BASE_URL + "/");
            HttpResult resp = HttpClient.get(
                    BASE_URL + "/view/" + ProviderUtil.urlEncode(date.trim()), h);
            return resp.isOk() ? resp.getBody() : "";
        } catch (Exception e) {
            return "";
        }
    }
}
