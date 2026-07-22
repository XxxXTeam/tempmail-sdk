package com.xxxxteam.tempmail.providers;

import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Normalizer;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Anonbox 渠道 — https://anonbox.net (CCC)
 * GET /en/ 解析 HTML 获取邮箱，HTTP mbox 格式取信。
 */
public final class Anonbox {

    private static final String CHANNEL = "anonbox";
    private static final String PAGE_URL = "https://anonbox.net/en/";
    private static final String BASE = "https://anonbox.net";

    private static final Pattern MAIL_LINK = Pattern.compile(
            "<a href=\"https://anonbox\\.net/([a-z0-9-]+)/([A-Za-z0-9._~-]+)\">https://anonbox\\.net/[^\"]+</a>",
            Pattern.CASE_INSENSITIVE);
    private static final Pattern DD_RE = Pattern.compile("(?is)<dd([^>]*)>([\\s\\S]*?)</dd>");
    private static final Pattern DISPLAY_NONE = Pattern.compile("display\\s*:\\s*none", Pattern.CASE_INSENSITIVE);
    private static final Pattern P_RE = Pattern.compile("(?is)<p>([\\s\\S]*?)</p>");
    private static final Pattern SPAN_RE = Pattern.compile("(?is)<span\\b[^>]*>[\\s\\S]*?</span>");
    private static final Pattern TAG_RE = Pattern.compile("<[^>]+>");
    private static final Pattern EXPIRES_RE = Pattern.compile(
            "(?is)Your mail address is valid until:</dt>\\s*<dd><p>([^<]+)</p>");

    private Anonbox() {
    }

    private static Map<String, String> htmlHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
        h.put("Accept", "text/html,application/xhtml+xml");
        return h;
    }

    private static Map<String, String> mboxHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
        h.put("Accept", "text/plain,*/*");
        return h;
    }

    private static String stripTags(String html) {
        String s = TAG_RE.matcher(html).replaceAll("");
        return s.replace("&nbsp;", " ").replace("&amp;", "&")
                .replace("&lt;", "<").replace("&gt;", ">").trim();
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult r = HttpClient.get(PAGE_URL, htmlHeaders());
        r.ensureSuccess();
        String[] parsed = parseEnPage(r.getBody());
        String email = parsed[0];
        String token = parsed[1];
        String exp = parsed[2];
        return new EmailInfo(CHANNEL, email, token, null, exp);
    }

    /**
     * 获取邮件列表（mbox 格式解析）。
     *
     * @param token 路径令牌 "{inbox}/{secret}"
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) {
            throw new IllegalArgumentException("anonbox: token 不能为空");
        }
        String path = token.endsWith("/") ? token : token + "/";
        HttpResult r = HttpClient.get(BASE + "/" + path, mboxHeaders());
        if (r.getStatusCode() == 404) return new ArrayList<>();
        r.ensureSuccess();

        String raw = r.getBody().trim();
        if (raw.isEmpty()) return new ArrayList<>();

        // 按 mbox 分割
        String[] blocks = raw.split("(?m)(?=^From )");
        List<Email> emails = new ArrayList<>();
        for (String block : blocks) {
            block = block.trim();
            if (block.isEmpty()) continue;
            Map<String, Object> rawMap = mboxBlockToRaw(block, email);
            emails.add(Normalizer.normalizeEmail(rawMap, email));
        }
        return emails;
    }

    private static String[] parseEnPage(String html) {
        Matcher m = MAIL_LINK.matcher(html);
        if (!m.find()) {
            throw new RuntimeException("anonbox: mailbox link not found");
        }
        String inbox = m.group(1);
        String secret = m.group(2);
        String token = inbox + "/" + secret;

        // 查找邮箱地址
        String addressDisplay = null;
        Matcher ddMatcher = DD_RE.matcher(html);
        while (ddMatcher.find()) {
            String attrs = ddMatcher.group(1);
            String inner = ddMatcher.group(2);
            if (DISPLAY_NONE.matcher(attrs).find()) continue;
            Matcher pm = P_RE.matcher(inner);
            if (!pm.find()) continue;
            String pInner = SPAN_RE.matcher(pm.group(1)).replaceAll("");
            String display = stripTags(pInner);
            if (!display.contains("@")) continue;
            int at = display.lastIndexOf('@');
            String domain = display.substring(at + 1).trim().toLowerCase();
            if ("googlemail.com".equals(domain)) continue;
            String expectedDomain = (inbox + ".anonbox.net").toLowerCase();
            if (!domain.equals(expectedDomain)) continue;
            String local = display.substring(0, at).trim();
            if (local.isEmpty()) continue;
            addressDisplay = display;
            break;
        }
        if (addressDisplay == null) {
            throw new RuntimeException("anonbox: address paragraph not found");
        }

        int at = addressDisplay.indexOf('@');
        String local = addressDisplay.substring(0, at).trim();
        if (local.isEmpty()) {
            throw new RuntimeException("anonbox: empty local part");
        }
        String emailAddr = local + "@" + inbox + ".anonbox.net";

        // 提取过期时间
        Matcher em = EXPIRES_RE.matcher(html);
        String exp = em.find() ? em.group(1).trim() : null;

        return new String[]{emailAddr, token, exp};
    }

    private static Map<String, Object> mboxBlockToRaw(String block, String recipient) {
        String normalized = block.replace("\r\n", "\n");
        String[] lines = normalized.split("\n");
        int i = 0;
        if (lines.length > 0 && lines[0].startsWith("From ")) i = 1;

        // 解析头部
        Map<String, String> headers = new LinkedHashMap<>();
        String curKey = "";
        while (i < lines.length) {
            String line = lines[i];
            if (line.isEmpty()) { i++; break; }
            if ((line.charAt(0) == ' ' || line.charAt(0) == '\t') && !curKey.isEmpty()) {
                headers.put(curKey, headers.get(curKey) + " " + line.trim());
            } else {
                int idx = line.indexOf(':');
                if (idx > 0) {
                    curKey = line.substring(0, idx).trim().toLowerCase();
                    headers.put(curKey, line.substring(idx + 1).trim());
                }
            }
            i++;
        }

        // 解析正文
        StringBuilder bodyBuilder = new StringBuilder();
        while (i < lines.length) {
            if (bodyBuilder.length() > 0) bodyBuilder.append("\n");
            bodyBuilder.append(lines[i]);
            i++;
        }
        String body = bodyBuilder.toString();

        String ct = headers.getOrDefault("content-type", "").toLowerCase();
        String text = "";
        String htmlBody = "";

        if (ct.contains("multipart/")) {
            Matcher bm = Pattern.compile("(?i)boundary=\"?([^\"\\s;]+)\"?").matcher(ct);
            if (bm.find()) {
                String boundary = bm.group(1);
                String[] parts = body.split("--" + Pattern.quote(boundary));
                for (String part : parts) {
                    part = part.trim();
                    if (part.isEmpty() || part.equals("--")) continue;
                    int sep = part.indexOf("\n\n");
                    if (sep < 0) continue;
                    String ph = part.substring(0, sep);
                    String pb = part.substring(sep + 2);
                    String pct = "";
                    Matcher pctM = Pattern.compile("(?i)^content-type:\\s*([^\\s;]+)", Pattern.MULTILINE).matcher(ph);
                    if (pctM.find()) pct = pctM.group(1).toLowerCase();
                    if ("text/plain".equals(pct)) text = pb.trim();
                    else if ("text/html".equals(pct)) htmlBody = pb.trim();
                }
            }
        }
        if (text.isEmpty() && htmlBody.isEmpty()) {
            if (ct.contains("text/html")) htmlBody = body.trim();
            else text = body.trim();
        }

        String dateStr = headers.getOrDefault("date", "");
        String msgId = headers.getOrDefault("message-id", simpleHash(block.substring(0, Math.min(512, block.length()))));

        Map<String, Object> raw = new LinkedHashMap<>();
        raw.put("id", msgId);
        raw.put("from", headers.getOrDefault("from", ""));
        raw.put("to", headers.getOrDefault("to", recipient));
        raw.put("subject", headers.getOrDefault("subject", ""));
        raw.put("text", text);
        raw.put("html", htmlBody);
        raw.put("date", dateStr);
        return raw;
    }

    private static String simpleHash(String s) {
        long h = 0;
        for (int i = 0; i < s.length(); i++) {
            h = (h * 31 + s.charAt(i)) & 0xFFFFFFFFL;
        }
        if (h == 0) return "0";
        String digits = "0123456789abcdefghijklmnopqrstuvwxyz";
        StringBuilder out = new StringBuilder();
        while (h > 0) {
            out.append(digits.charAt((int) (h % 36)));
            h /= 36;
        }
        return out.reverse().toString();
    }
}
