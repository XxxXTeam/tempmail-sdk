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
 * 10minutemail.net 渠道 — PHPSESSID session + HTML 表格解析。
 */
public final class TenminutemailNet {

    private static final String CHANNEL = "10minutemail-net";
    private static final String BASE_URL = "https://10minutemail.net";
    private static final String TOK_PREFIX = "tmn1:";

    private static final Pattern EMAIL_RE = Pattern.compile("value=\"([^\"]+@[^\"]+)\"");
    private static final Pattern ROW_RE = Pattern.compile("(?is)<tr[^>]*>(.*?)</tr>");
    private static final Pattern MID_RE = Pattern.compile("(?i)readmail\\.html\\?mid=([^'\"&]+)");
    private static final Pattern CELL_RE = Pattern.compile("(?is)<td[^>]*>(.*?)</td>");
    private static final Pattern TITLE_RE = Pattern.compile("(?i)title=\"([^\"]+)\"");
    private static final Pattern CF_RE = Pattern.compile("(?i)data-cfemail=\"([0-9a-fA-F]+)\"");
    private static final Pattern TAG_RE = Pattern.compile("(?s)<[^>]+>");

    private static final String UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private TenminutemailNet() {
    }

    private static Map<String, String> browserHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        h.put("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7");
        return h;
    }

    private static Map<String, String> ajaxHeaders(String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "*/*");
        h.put("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Referer", BASE_URL + "/");
        if (cookie != null && !cookie.isEmpty()) {
            h.put("Cookie", cookie);
        }
        return h;
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
        String raw = Json.serialize(Map.of("c", cookieHdr));
        return TOK_PREFIX + Base64.getEncoder().encodeToString(raw.getBytes(StandardCharsets.UTF_8));
    }

    private static String decodeToken(String token) {
        if (token == null || !token.startsWith(TOK_PREFIX)) return "";
        try {
            byte[] decoded = Base64.getDecoder().decode(token.substring(TOK_PREFIX.length()));
            com.google.gson.JsonElement el = Json.parse(new String(decoded, StandardCharsets.UTF_8));
            return Json.str(el, "c");
        } catch (Exception e) {
            return "";
        }
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(BASE_URL + "/", browserHeaders());
        resp.ensureSuccess();
        String cookieHdr = extractCookieHeader(resp);

        Matcher m = EMAIL_RE.matcher(resp.getBody());
        if (!m.find()) {
            throw new RuntimeException("10minutemail-net: 未能从首页提取邮箱地址");
        }
        String email = m.group(1).trim();
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("10minutemail-net: 获取到的邮箱地址无效: " + email);
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
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("10minutemail-net: 邮箱地址为空");
        }
        String addr = email.trim();
        String cookieHdr = decodeToken(token != null ? token.trim() : "");

        String listUrl = BASE_URL + "/mailbox.ajax.php?_=" + System.currentTimeMillis();
        HttpResult resp = HttpClient.get(listUrl, ajaxHeaders(cookieHdr));
        resp.ensureSuccess();

        List<Email> out = new ArrayList<>();
        Matcher rowMatcher = ROW_RE.matcher(resp.getBody());
        while (rowMatcher.find()) {
            String rowInner = rowMatcher.group(1);
            if (rowInner.toLowerCase().contains("<th")) continue;

            Matcher midMatch = MID_RE.matcher(rowInner);
            if (!midMatch.find()) continue;
            String mailId = midMatch.group(1).trim();
            if (mailId.isEmpty()) continue;

            // 提取单元格
            List<String> cells = new ArrayList<>();
            Matcher cellMatcher = CELL_RE.matcher(rowInner);
            while (cellMatcher.find()) {
                cells.add(cellMatcher.group(1));
            }
            String fromCell = cells.size() > 0 ? cells.get(0) : "";
            String subjectCell = cells.size() > 1 ? cells.get(1) : "";
            String dateCell = cells.size() > 2 ? cells.get(2) : "";

            String fromAddr = extractText(fromCell);
            String subject = extractText(subjectCell);

            // 日期优先取 title 属性
            Matcher tm = TITLE_RE.matcher(dateCell);
            String date = tm.find() ? tm.group(1).trim() : extractText(dateCell);

            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", mailId);
            raw.put("from", fromAddr);
            raw.put("to", addr);
            raw.put("subject", subject);
            raw.put("html", fetchBody(cookieHdr, mailId));
            raw.put("date", date);
            out.add(Normalizer.normalizeEmail(raw, addr));
        }
        return out;
    }

    private static String fetchBody(String cookieHdr, String mailId) {
        if (mailId.isEmpty()) return "";
        Map<String, String> h = browserHeaders();
        h.put("Referer", BASE_URL + "/");
        if (cookieHdr != null && !cookieHdr.isEmpty()) {
            h.put("Cookie", cookieHdr);
        }
        try {
            HttpResult resp = HttpClient.get(BASE_URL + "/readmail.html?mid=" + ProviderUtil.urlEncode(mailId), h);
            if (!resp.isOk()) return "";
            return extractBody(resp.getBody());
        } catch (RuntimeException e) {
            return "";
        }
    }

    private static String extractBody(String page) {
        String startMark = "class=\"mailinhtml\"";
        int si = page.indexOf(startMark);
        if (si < 0) return "";
        int gt = page.indexOf(">", si);
        if (gt < 0) return "";
        String rest = page.substring(gt + 1);

        int ei = rest.indexOf("email-decode.min.js");
        if (ei < 0) {
            int di = rest.indexOf("</div>");
            return di >= 0 ? rest.substring(0, di).trim() : rest.trim();
        }
        String segment = rest.substring(0, ei);
        int sIdx = segment.lastIndexOf("<script");
        if (sIdx >= 0) segment = segment.substring(0, sIdx);
        segment = segment.trim();
        // 去掉尾部两个 </div>
        for (int k = 0; k < 2; k++) {
            segment = segment.trim();
            if (segment.endsWith("</div>")) {
                segment = segment.substring(0, segment.length() - 6);
            }
        }
        return segment.trim();
    }

    private static String extractText(String cell) {
        // 优先解码 Cloudflare 邮箱保护混淆
        Matcher cf = CF_RE.matcher(cell);
        if (cf.find()) {
            String decoded = cfDecode(cf.group(1));
            if (!decoded.isEmpty()) return decoded;
        }
        String text = TAG_RE.matcher(cell).replaceAll("");
        return text.replace("&nbsp;", " ").replace("&#160;", " ")
                .replace("&amp;", "&").replace("&lt;", "<")
                .replace("&gt;", ">").replace("&quot;", "\"").trim();
    }

    private static String cfDecode(String encoded) {
        byte[] data;
        try {
            data = hexToBytes(encoded);
        } catch (Exception e) {
            return "";
        }
        if (data.length < 2) return "";
        int key = data[0] & 0xFF;
        byte[] decoded = new byte[data.length - 1];
        for (int i = 1; i < data.length; i++) {
            decoded[i - 1] = (byte) ((data[i] & 0xFF) ^ key);
        }
        String result = new String(decoded, StandardCharsets.UTF_8);
        return result.contains("@") ? result : "";
    }

    private static byte[] hexToBytes(String hex) {
        int len = hex.length() / 2;
        byte[] data = new byte[len];
        for (int i = 0; i < len; i++) {
            data[i] = (byte) Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16);
        }
        return data;
    }
}
