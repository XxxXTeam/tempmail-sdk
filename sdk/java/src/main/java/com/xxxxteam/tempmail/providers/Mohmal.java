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
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Mohmal 渠道 — https://www.mohmal.com
 * 基于 HTML 解析的临时邮箱，使用 connect.sid session cookie 维持会话。
 */
public final class Mohmal {

    private static final String CHANNEL = "mohmal";
    private static final String ORIGIN = "https://www.mohmal.com";
    private static final String TOK_PREFIX = "moh1:";

    private static final Pattern DATA_EMAIL_RE = Pattern.compile("data-email=\"([^\"]+)\"");
    private static final Pattern MESSAGE_LINK_RE = Pattern.compile("/en/message/(\\d+)");
    private static final Pattern MESSAGE_BODY_RE = Pattern.compile(
            "(?is)<div[^>]*class=\"[^\"]*mail-content[^\"]*\"[^>]*>([\\s\\S]*?)</div>");
    private static final Pattern MESSAGE_BODY_ALT_RE = Pattern.compile(
            "(?is)<div[^>]*class=\"[^\"]*message-body[^\"]*\"[^>]*>([\\s\\S]*?)</div>");
    private static final Pattern DETAIL_FROM_RE = Pattern.compile(
            "(?is)<span[^>]*class=\"[^\"]*from[^\"]*\"[^>]*>([\\s\\S]*?)</span>");
    private static final Pattern DETAIL_SUBJECT_RE = Pattern.compile(
            "(?is)<span[^>]*class=\"[^\"]*subject[^\"]*\"[^>]*>([\\s\\S]*?)</span>");
    private static final Pattern DETAIL_DATE_RE = Pattern.compile(
            "(?is)<span[^>]*class=\"[^\"]*date[^\"]*\"[^>]*>([\\s\\S]*?)</span>");
    private static final Pattern FROM_ADDR_RE = Pattern.compile(
            "[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,}");
    private static final Pattern TAG_RE = Pattern.compile("<[^>]+>");

    private Mohmal() {
    }

    private static Map<String, String> pageHeaders(String referer, String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        h.put("Accept-Language", "en-US,en;q=0.9");
        h.put("Cache-Control", "no-cache");
        h.put("DNT", "1");
        h.put("Pragma", "no-cache");
        h.put("Upgrade-Insecure-Requests", "1");
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        h.put("Referer", referer);
        if (cookie != null && !cookie.isEmpty()) {
            h.put("Cookie", cookie);
        }
        return h;
    }

    private static String stripTags(String s) {
        return TAG_RE.matcher(s).replaceAll("").replaceAll("&nbsp;", " ")
                .replaceAll("&amp;", "&").replaceAll("&lt;", "<")
                .replaceAll("&gt;", ">").trim();
    }

    private static String encodeToken(String cookieHdr) {
        String raw = Json.serialize(Map.of("c", cookieHdr));
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.getBytes(StandardCharsets.UTF_8));
    }

    private static String decodeToken(String tok) {
        if (tok == null || !tok.startsWith(TOK_PREFIX)) {
            throw new IllegalArgumentException("mohmal: 无效的 session token");
        }
        try {
            byte[] decoded = Base64.getDecoder().decode(tok.substring(TOK_PREFIX.length()));
            com.google.gson.JsonElement el = Json.parse(new String(decoded, StandardCharsets.UTF_8));
            String c = Json.str(el, "c");
            if (c.isEmpty()) throw new IllegalArgumentException("mohmal: token 中 cookie 为空");
            return c;
        } catch (IllegalArgumentException e) {
            throw e;
        } catch (Exception e) {
            throw new IllegalArgumentException("mohmal: 无效的 session token", e);
        }
    }

    private static String mergeCookies(String prev, HttpResult resp) {
        Map<String, String> cookies = new LinkedHashMap<>();
        if (prev != null && !prev.isEmpty()) {
            for (String part : prev.split(";")) {
                part = part.trim();
                int eq = part.indexOf('=');
                if (eq > 0) {
                    cookies.put(part.substring(0, eq).trim(), part.substring(eq + 1).trim());
                }
            }
        }
        for (String sc : resp.getSetCookies()) {
            String nv = sc.split(";")[0].trim();
            int eq = nv.indexOf('=');
            if (eq > 0) {
                cookies.put(nv.substring(0, eq).trim(), nv.substring(eq + 1).trim());
            }
        }
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> kv : cookies.entrySet()) {
            if (sb.length() > 0) sb.append("; ");
            sb.append(kv.getKey()).append("=").append(kv.getValue());
        }
        return sb.toString();
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        // 不跟随重定向以捕获 session cookie
        HttpResult r1 = HttpClient.sendNoRedirect("GET", ORIGIN + "/en/create/random",
                pageHeaders(ORIGIN, null), null, null, null);
        String cookieHdr = mergeCookies("", r1);

        // 手动跟随重定向到 inbox
        HttpResult r2 = HttpClient.get(ORIGIN + "/en/inbox",
                pageHeaders(ORIGIN + "/en/create/random", cookieHdr));
        r2.ensureSuccess();
        cookieHdr = mergeCookies(cookieHdr, r2);
        String pageHtml = r2.getBody();

        // 验证 connect.sid
        if (!cookieHdr.contains("connect.sid")) {
            throw new RuntimeException("mohmal: 缺少 connect.sid session cookie");
        }

        // 提取邮箱地址
        Matcher m = DATA_EMAIL_RE.matcher(pageHtml);
        if (!m.find()) {
            throw new RuntimeException("mohmal: 无法从 inbox 页面提取邮箱地址");
        }
        String email = m.group(1).replaceAll("&amp;", "&").trim();
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("mohmal: 无效的邮箱地址: " + email);
        }

        return new EmailInfo(CHANNEL, email, encodeToken(cookieHdr), null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 会话令牌
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String cookieHdr = decodeToken(token);
        String addr = (email != null) ? email.trim() : "";

        // 获取收件箱页面
        HttpResult r = HttpClient.get(ORIGIN + "/en/inbox", pageHeaders(ORIGIN, cookieHdr));
        r.ensureSuccess();

        // 提取所有邮件 ID
        Set<String> seen = new LinkedHashSet<>();
        Matcher midMatcher = MESSAGE_LINK_RE.matcher(r.getBody());
        while (midMatcher.find()) {
            seen.add(midMatcher.group(1));
        }
        if (seen.isEmpty()) return new ArrayList<>();

        // 逐条获取详情
        List<Email> result = new ArrayList<>();
        for (String mid : seen) {
            HttpResult rd = HttpClient.get(ORIGIN + "/en/message/" + mid,
                    pageHeaders(ORIGIN + "/en/inbox", cookieHdr));
            if (!rd.isOk()) continue;
            Map<String, Object> raw = parseMessageDetail(rd.getBody(), mid, addr);
            result.add(Normalizer.normalizeEmail(raw, addr));
        }
        return result;
    }

    private static Map<String, Object> parseMessageDetail(String page, String mid, String recipient) {
        Map<String, Object> raw = new LinkedHashMap<>();
        raw.put("id", mid);
        raw.put("to", recipient);

        // 提取发件人
        Matcher fm = DETAIL_FROM_RE.matcher(page);
        if (fm.find()) {
            String fromRaw = fm.group(1);
            Matcher em = FROM_ADDR_RE.matcher(fromRaw);
            raw.put("from", em.find() ? em.group(0) : stripTags(fromRaw));
        }

        // 提取主题
        Matcher sm = DETAIL_SUBJECT_RE.matcher(page);
        if (sm.find()) raw.put("subject", stripTags(sm.group(1)));

        // 提取日期
        Matcher dm = DETAIL_DATE_RE.matcher(page);
        if (dm.find()) raw.put("date", stripTags(dm.group(1)));

        // 提取正文
        Matcher bm = MESSAGE_BODY_RE.matcher(page);
        if (!bm.find()) bm = MESSAGE_BODY_ALT_RE.matcher(page);
        if (bm.find()) raw.put("html", bm.group(1).trim());

        return raw;
    }
}
