package com.xxxxteam.tempmail.providers;

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
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Lroid 渠道 — https://lroid.com
 * HTML 解析模式，使用 session cookies 维持邮箱身份。
 */
public final class Lroid {

    private static final String CHANNEL = "lroid";
    private static final String BASE_URL = "https://lroid.com";

    private static final Pattern EMAIL_RE = Pattern.compile(
            "<input[^>]+id=[\"']eposta_adres[\"'][^>]+value=[\"']([^\"']+@[^\"']+)[\"']",
            Pattern.CASE_INSENSITIVE);
    private static final Pattern EMAIL_RE_ALT = Pattern.compile(
            "<input[^>]+value=[\"']([^\"']+@[^\"']+)[\"'][^>]+id=[\"']eposta_adres[\"']",
            Pattern.CASE_INSENSITIVE);
    private static final Pattern TAG_RE = Pattern.compile("<[^>]+>");

    private Lroid() {
    }

    private static Map<String, String> pageHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        h.put("Accept-Language", "en-US,en;q=0.9");
        h.put("Referer", BASE_URL + "/");
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        return h;
    }

    private static String extractEmail(String html) {
        Matcher m = EMAIL_RE.matcher(html);
        if (m.find()) {
            String addr = m.group(1).trim();
            if (addr.contains("@")) return addr;
        }
        m = EMAIL_RE_ALT.matcher(html);
        if (m.find()) {
            String addr = m.group(1).trim();
            if (addr.contains("@")) return addr;
        }
        throw new RuntimeException("lroid: 无法从 HTML 响应中解析邮箱地址");
    }

    private static String extractCookieHeader(HttpResult resp) {
        StringBuilder sb = new StringBuilder();
        for (String sc : resp.getSetCookies()) {
            String nv = sc.split(";")[0].trim();
            if (!nv.isEmpty() && nv.contains("=")) {
                if (sb.length() > 0) sb.append("; ");
                sb.append(nv);
            }
        }
        return sb.toString();
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(BASE_URL, pageHeaders());
        resp.ensureSuccess();
        String email = extractEmail(resp.getBody());
        String cookieHeader = extractCookieHeader(resp);

        // 将 cookie 序列化为 JSON token
        Map<String, String> tokenData = new LinkedHashMap<>();
        tokenData.put("cookie", cookieHeader);
        String token = Json.serialize(tokenData);
        return new EmailInfo(CHANNEL, email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 字符串 {"cookie":"..."}
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) {
            throw new IllegalArgumentException("lroid: token 不能为空");
        }
        String addr = (email != null) ? email.trim() : "";
        if (addr.isEmpty()) {
            throw new IllegalArgumentException("lroid: 邮箱地址不能为空");
        }

        // 解析 token 提取 cookie
        JsonElement parsed = Json.parse(token);
        String cookie = Json.str(parsed, "cookie");
        if (cookie.isEmpty()) {
            throw new IllegalArgumentException("lroid: token 中缺少 cookie");
        }

        // 尝试 kontrol API
        try {
            Map<String, String> h = new LinkedHashMap<>();
            h.put("Accept", "application/json, text/html, */*;q=0.8");
            h.put("Referer", BASE_URL + "/");
            h.put("Cookie", cookie);
            h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                    + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
            HttpResult resp = HttpClient.get(BASE_URL + "/en/api-kontrol/", h);
            if (resp.isOk()) {
                JsonElement data = Json.parse(resp.getBody());
                if (data != null && data.isJsonArray()) {
                    return parseJsonEmails(data.getAsJsonArray(), addr);
                }
                if (data != null && data.isJsonObject()) {
                    for (String key : new String[]{"mails", "emails", "messages", "data", "inbox"}) {
                        JsonElement items = data.getAsJsonObject().get(key);
                        if (items != null && items.isJsonArray()) {
                            return parseJsonEmails(items.getAsJsonArray(), addr);
                        }
                    }
                }
            }
        } catch (RuntimeException ignored) {
        }

        // 回退到 HTML 页面解析
        return parseHtmlEmails(cookie, addr);
    }

    private static List<Email> parseJsonEmails(com.google.gson.JsonArray items, String recipient) {
        List<Email> emails = new ArrayList<>();
        for (JsonElement item : items) {
            if (!item.isJsonObject()) continue;
            Map<String, Object> raw = Json.toDict(item);
            if (raw.containsKey("body") && !raw.containsKey("html")) {
                raw.put("html", raw.get("body"));
            }
            if (raw.containsKey("content") && !raw.containsKey("html")) {
                raw.put("html", raw.get("content"));
            }
            emails.add(Normalizer.normalizeEmail(raw, recipient));
        }
        return emails;
    }

    private static List<Email> parseHtmlEmails(String cookie, String recipient) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        h.put("Referer", BASE_URL + "/");
        h.put("Cookie", cookie);
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        HttpResult resp = HttpClient.get(BASE_URL, h);
        resp.ensureSuccess();

        List<Email> emails = new ArrayList<>();
        Pattern mailItemRe = Pattern.compile(
                "<li[^>]*class=[\"'][^\"']*\\bmail\\b[^\"']*[\"'][^>]*>(.*?)</li>",
                Pattern.DOTALL | Pattern.CASE_INSENSITIVE);
        Matcher itemMatcher = mailItemRe.matcher(resp.getBody());
        int idx = 0;
        while (itemMatcher.find()) {
            idx++;
            String itemHtml = itemMatcher.group(1);
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", String.valueOf(idx));
            raw.put("to", recipient);

            // 提取主题
            Pattern subjRe = Pattern.compile(
                    "class=[\"'][^\"']*\\bsubject\\b[^\"']*[\"'][^>]*>(.*?)</",
                    Pattern.DOTALL | Pattern.CASE_INSENSITIVE);
            Matcher sm = subjRe.matcher(itemHtml);
            if (sm.find()) raw.put("subject", TAG_RE.matcher(sm.group(1)).replaceAll("").trim());

            // 提取发件人
            Pattern fromRe = Pattern.compile(
                    "class=[\"'][^\"']*\\b(?:from|sender)\\b[^\"']*[\"'][^>]*>(.*?)</",
                    Pattern.DOTALL | Pattern.CASE_INSENSITIVE);
            Matcher fm = fromRe.matcher(itemHtml);
            if (fm.find()) raw.put("from", TAG_RE.matcher(fm.group(1)).replaceAll("").trim());

            // 提取日期
            Pattern dateRe = Pattern.compile(
                    "class=[\"'][^\"']*\\b(?:date|time)\\b[^\"']*[\"'][^>]*>(.*?)</",
                    Pattern.DOTALL | Pattern.CASE_INSENSITIVE);
            Matcher dm = dateRe.matcher(itemHtml);
            if (dm.find()) raw.put("date", TAG_RE.matcher(dm.group(1)).replaceAll("").trim());

            emails.add(Normalizer.normalizeEmail(raw, recipient));
        }
        return emails;
    }
}
