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

import java.time.Instant;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Tempmail365 渠道实现 — https://tempmail365.cn
 */
public final class Tempmail365 {

    private static final String BASE = "https://tempmail365.cn/tempemail.php";
    private static final List<String> FALLBACK_DOMAINS = List.of(
            "fengyou.cc", "shop345.com", "nutemail.com", "qvrf.cn");

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json, text/plain, */*");
        HEADERS.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        HEADERS.put("Cache-Control", "no-cache");
        HEADERS.put("DNT", "1");
        HEADERS.put("Pragma", "no-cache");
        HEADERS.put("Referer", "https://tempmail365.cn/");
        HEADERS.put("sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"");
        HEADERS.put("sec-ch-ua-mobile", "?0");
        HEADERS.put("sec-ch-ua-platform", "\"Windows\"");
        HEADERS.put("sec-fetch-dest", "empty");
        HEADERS.put("sec-fetch-mode", "cors");
        HEADERS.put("sec-fetch-site", "same-origin");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    }

    private Tempmail365() {
    }

    /**
     * 创建临时邮箱（支持指定域名）。
     *
     * @param domain 指定域名，null 或空串则随机选取
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        List<String> domains = fetchDomains();
        if (domains.isEmpty()) {
            throw new RuntimeException("tempmail365: 无可用域名");
        }
        String selected;
        if (domain != null && !domain.trim().isEmpty()) {
            String d = domain.trim().toLowerCase();
            if (domains.stream().noneMatch(x -> x.equalsIgnoreCase(d))) {
                throw new RuntimeException("tempmail365: 域名不可用: " + d);
            }
            selected = d;
        } else {
            selected = domains.get(ThreadLocalRandom.current().nextInt(domains.size()));
        }
        String username = ProviderUtil.randomString(8);
        String addr = username + "@" + selected;

        HttpResult resp = HttpClient.get(
                BASE + "?action=create_email&email=" + ProviderUtil.urlEncode(addr)
                        + "&domain=" + ProviderUtil.urlEncode(selected), HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonObject()) {
            throw new RuntimeException("tempmail365: 创建邮箱失败");
        }
        JsonObject root = data.getAsJsonObject();
        if (!root.has("success") || !root.get("success").getAsBoolean()) {
            throw new RuntimeException("tempmail365: 创建邮箱失败");
        }
        return new EmailInfo("tempmail365", addr);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.trim().isEmpty()) {
            throw new RuntimeException("tempmail365: 邮箱地址为空");
        }
        String e = email.trim();
        HttpResult resp = HttpClient.get(
                BASE + "?action=fetch_mail&email=" + ProviderUtil.urlEncode(e), HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonObject()) {
            return new ArrayList<>();
        }
        JsonObject root = data.getAsJsonObject();
        String content = Json.str(root, "content");
        if (content.isEmpty() || "无邮件".equals(content.trim())) {
            return new ArrayList<>();
        }
        // 从 HTML 内容中提取发件人和主题
        String sender = extractSender(content);
        String subject = extractSubject(content);
        Map<String, Object> raw = new LinkedHashMap<>();
        raw.put("from", sender);
        raw.put("subject", subject);
        raw.put("body", content);
        raw.put("date", DateTimeFormatter.ISO_INSTANT.format(Instant.now()));
        List<Email> result = new ArrayList<>();
        result.add(Normalizer.normalizeEmail(raw, e));
        return result;
    }

    private static List<String> fetchDomains() {
        try {
            HttpResult resp = HttpClient.get(BASE + "?action=get_config", HEADERS);
            resp.ensureSuccess();
            JsonElement data = Json.parse(resp.getBody());
            if (data == null || !data.isJsonObject()) {
                return new ArrayList<>(FALLBACK_DOMAINS);
            }
            JsonArray arr = Json.arr(data, "domains");
            if (arr == null || arr.isEmpty()) {
                return new ArrayList<>(FALLBACK_DOMAINS);
            }
            List<String> out = new ArrayList<>();
            for (JsonElement el : arr) {
                if (el.isJsonPrimitive()) {
                    String d = el.getAsString().trim();
                    if (!d.isEmpty()) {
                        out.add(d);
                    }
                }
            }
            return out.isEmpty() ? new ArrayList<>(FALLBACK_DOMAINS) : out;
        } catch (RuntimeException e) {
            return new ArrayList<>(FALLBACK_DOMAINS);
        }
    }

    private static String extractSender(String content) {
        Matcher m = Pattern.compile("(?:发件人|From)\\s*[:：]\\s*(.+?)(?:<br|</|<p|\\n|\\r)",
                Pattern.CASE_INSENSITIVE).matcher(content);
        return m.find() ? m.group(1).trim() : "";
    }

    private static String extractSubject(String content) {
        Matcher m = Pattern.compile("(?:主题|Subject)\\s*[:：]\\s*(.+?)(?:<br|</|<p|\\n|\\r)",
                Pattern.CASE_INSENSITIVE).matcher(content);
        return m.find() ? m.group(1).trim() : "";
    }
}