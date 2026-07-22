package com.xxxxteam.tempmail.providers;

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
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Haribu 渠道 — https://haribu.net
 * Tempail 类模式，session cookies 维持会话。
 */
public final class Haribu {

    private static final String CHANNEL = "haribu";
    private static final String BASE = "https://haribu.net";
    private static final String TOK_PREFIX = "haribu1:";

    private static final Pattern EMAIL_INPUT_RE = Pattern.compile(
            "(?i)<input[^>]+id\\s*=\\s*[\"']eposta_adres[\"'][^>]+value\\s*=\\s*[\"']([^\"']+)[\"']");
    private static final Pattern EMAIL_INPUT_RE2 = Pattern.compile(
            "(?i)<input[^>]+value\\s*=\\s*[\"']([^\"']+@[^\"']+)[\"'][^>]+id\\s*=\\s*[\"']eposta_adres[\"']");
    private static final Pattern MAIL_ITEM_RE = Pattern.compile(
            "(?is)<li\\s+class\\s*=\\s*[\"']mail[\"'][^>]*>([\\s\\S]*?)</li>");
    private static final Pattern FROM_RE = Pattern.compile(
            "(?is)<span\\s+class\\s*=\\s*[\"']mail_gonderen[\"'][^>]*>([\\s\\S]*?)</span>");
    private static final Pattern SUBJECT_RE = Pattern.compile(
            "(?is)<span\\s+class\\s*=\\s*[\"']mail_konu[\"'][^>]*>([\\s\\S]*?)</span>");
    private static final Pattern DATE_RE = Pattern.compile(
            "(?is)<span\\s+class\\s*=\\s*[\"']mail_zaman[\"'][^>]*>([\\s\\S]*?)</span>");
    private static final Pattern MAIL_LINK_RE = Pattern.compile(
            "(?i)href\\s*=\\s*[\"']([^\"']*(?:mail|read|view)[^\"']*)[\"']");
    private static final Pattern BODY_RE = Pattern.compile(
            "(?is)<div\\s+(?:id|class)\\s*=\\s*[\"'](?:mail_icerik|icerik|mail-content|message-body)[\"'][^>]*>([\\s\\S]*?)</div>");
    private static final Pattern TAG_RE = Pattern.compile("<[^>]+>");

    private Haribu() {
    }

    private static Map<String, String> defaultHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        h.put("Cache-Control", "no-cache");
        h.put("DNT", "1");
        h.put("Pragma", "no-cache");
        h.put("Upgrade-Insecure-Requests", "1");
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        return h;
    }

    private static String stripTags(String s) {
        return TAG_RE.matcher(s).replaceAll(" ").trim();
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

    private static String encodeToken(String cookieHdr) {
        String payload = Json.serialize(Map.of("c", cookieHdr));
        return TOK_PREFIX + Base64.getEncoder().encodeToString(payload.getBytes(StandardCharsets.UTF_8));
    }

    private static String decodeToken(String token) {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw new IllegalArgumentException("haribu: 无效的会话令牌");
        }
        try {
            byte[] decoded = Base64.getDecoder().decode(token.substring(TOK_PREFIX.length()));
            com.google.gson.JsonElement el = Json.parse(new String(decoded, StandardCharsets.UTF_8));
            String c = Json.str(el, "c");
            if (c.isEmpty()) throw new IllegalArgumentException("haribu: 会话令牌中缺少 cookie");
            return c;
        } catch (IllegalArgumentException e) {
            throw e;
        } catch (Exception e) {
            throw new IllegalArgumentException("haribu: 无效的会话令牌", e);
        }
    }

    private static String extractEmail(String html) {
        Matcher m = EMAIL_INPUT_RE.matcher(html);
        if (m.find()) {
            String addr = m.group(1).trim();
            if (addr.contains("@")) return addr;
        }
        m = EMAIL_INPUT_RE2.matcher(html);
        if (m.find()) {
            String addr = m.group(1).trim();
            if (addr.contains("@")) return addr;
        }
        throw new RuntimeException("haribu: 未找到邮箱地址（eposta_adres）");
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(BASE, defaultHeaders());
        resp.ensureSuccess();
        String html = resp.getBody();
        if (html == null || html.isEmpty()) {
            throw new RuntimeException("haribu: 首页响应为空");
        }
        String email = extractEmail(html);
        String cookieHdr = extractCookieHeader(resp);
        String token = encodeToken(cookieHdr);
        return new EmailInfo(CHANNEL, email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token 会话令牌
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("haribu: 邮箱地址为空");
        }
        String e = email.trim();
        String cookieHdr = decodeToken(token);

        // 调用 kontrol API 触发新邮件检查
        try {
            Map<String, String> h = defaultHeaders();
            h.put("Cookie", cookieHdr);
            h.put("Referer", BASE);
            h.put("X-Requested-With", "XMLHttpRequest");
            HttpClient.get(BASE + "/en/api-kontrol/", h);
        } catch (RuntimeException ignored) {
        }

        // GET 首页获取收件箱页面
        Map<String, String> h = defaultHeaders();
        h.put("Cookie", cookieHdr);
        h.put("Referer", BASE);
        HttpResult resp = HttpClient.get(BASE, h);
        resp.ensureSuccess();

        List<Email> emails = new ArrayList<>();
        Matcher itemMatcher = MAIL_ITEM_RE.matcher(resp.getBody());
        int idx = 0;
        while (itemMatcher.find()) {
            String content = itemMatcher.group(1);
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", "haribu-" + idx);
            raw.put("to", e);
            idx++;

            Matcher fm = FROM_RE.matcher(content);
            if (fm.find()) raw.put("from", stripTags(fm.group(1)).trim());

            Matcher sm = SUBJECT_RE.matcher(content);
            if (sm.find()) raw.put("subject", stripTags(sm.group(1)).trim());

            Matcher dm = DATE_RE.matcher(content);
            if (dm.find()) raw.put("date", stripTags(dm.group(1)).trim());

            // 尝试获取详情链接
            Matcher lm = MAIL_LINK_RE.matcher(content);
            if (lm.find()) {
                String detailUrl = lm.group(1);
                if (!detailUrl.startsWith("http")) {
                    detailUrl = BASE + "/" + detailUrl.replaceAll("^/+", "");
                }
                String htmlBody = fetchDetail(detailUrl, cookieHdr);
                if (!htmlBody.isEmpty()) {
                    raw.put("html", htmlBody);
                }
            }
            emails.add(Normalizer.normalizeEmail(raw, e));
        }
        return emails;
    }

    private static String fetchDetail(String url, String cookieHdr) {
        try {
            Map<String, String> h = defaultHeaders();
            h.put("Cookie", cookieHdr);
            h.put("Referer", BASE);
            HttpResult resp = HttpClient.get(url, h);
            if (!resp.isOk()) return "";
            Matcher m = BODY_RE.matcher(resp.getBody());
            if (m.find()) return m.group(1).trim();
        } catch (RuntimeException ignored) {
        }
        return "";
    }
}
