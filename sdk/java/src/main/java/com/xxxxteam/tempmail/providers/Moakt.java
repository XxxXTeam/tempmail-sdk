package com.xxxxteam.tempmail.providers;

import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
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
 * moakt.com 家族实现（HTML 抓取，Cookie 会话）。
 *
 * <p>流程：GET 首页取初始 Cookie → POST /inbox 创建邮箱（捕获 302 的 tm_session）
 * → GET /inbox 解析邮箱地址 → 收信时 GET /inbox 列出邮件 ID → 逐封 GET HTML 详情。</p>
 *
 * <p>Token 格式：{@code mok1:<base64({"l":"zh","c":"cookie_header"})>}</p>
 */
public final class Moakt {

    private static final String ORIGIN = "https://www.moakt.com";
    private static final String TOK_PREFIX = "mok1:";
    private static final String DEFAULT_UA =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    // HTML 解析正则
    private static final Pattern EMAIL_DIV_RE = Pattern.compile(
            "<div\\s+id=\"email-address\"\\s*>([^<]+)</div>",
            Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
    private static final Pattern DOMAIN_OPTION_RE = Pattern.compile(
            "<option\\s+value=\"([^\"]+)\">\\s*@[^<]+</option>",
            Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
    private static final Pattern HREF_EMAIL_RE = Pattern.compile(
            "href=\"(/[^\"/]+/email/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\"");
    private static final Pattern TITLE_RE = Pattern.compile(
            "<li\\s+class=\"title\"\\s*>([^<]*)</li>",
            Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
    private static final Pattern DATE_RE = Pattern.compile(
            "<li\\s+class=\"date\"[^>]*>[\\s\\S]*?<span[^>]*>([^<]+)</span>",
            Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
    private static final Pattern SENDER_RE = Pattern.compile(
            "<li\\s+class=\"sender\"[^>]*>[\\s\\S]*?<span[^>]*>([\\s\\S]*?)</span>\\s*</li>",
            Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
    private static final Pattern BODY_OPEN_RE = Pattern.compile(
            "<div\\s+class=\"email-body\"\\s*>",
            Pattern.CASE_INSENSITIVE);

    /**
     * 使用栈式深度匹配提取 email-body div 的完整内部 HTML，
     * 避免非贪婪正则在嵌套 div 时截断正文。
     */
    private static String extractBodyHtml(String page) {
        Matcher m = BODY_OPEN_RE.matcher(page);
        if (!m.find()) return "";
        int start = m.end();
        int pos = start;
        int depth = 1;
        while (pos < page.length() && depth > 0) {
            int nextOpen = page.indexOf("<div", pos);
            int nextClose = page.indexOf("</div>", pos);
            if (nextClose == -1) break;
            if (nextOpen != -1 && nextOpen < nextClose) {
                depth++;
                pos = nextOpen + 4;
            } else {
                depth--;
                if (depth == 0) {
                    return page.substring(start, nextClose).strip();
                }
                pos = nextClose + 6;
            }
        }
        return "";
    }
    private static final Pattern FROM_ADDR_RE = Pattern.compile(
            "<([a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.[a-zA-Z]{2,})>");
    private static final Pattern TAG_RE = Pattern.compile("<[^>]+>");
    private static final Pattern MAIL_DOMAIN_RE = Pattern.compile(
            "(?i)^[a-z0-9](?:[a-z0-9\\-]{0,61}[a-z0-9])?(?:\\.[a-z0-9](?:[a-z0-9\\-]{0,61}[a-z0-9])?)+$");

    private Moakt() {
    }

    // ── Cookie 工具 ──────────────────────────────────────────────────────────

    /** 从 Set-Cookie 原始值列表中提取 name=value 对，合并到现有 cookie 映射。 */
    private static Map<String, String> parseCookieMap(String cookieHdr) {
        Map<String, String> m = new LinkedHashMap<>();
        if (cookieHdr == null || cookieHdr.isBlank()) {
            return m;
        }
        for (String part : cookieHdr.split(";")) {
            part = part.strip();
            int eq = part.indexOf('=');
            if (eq <= 0) {
                continue;
            }
            String k = part.substring(0, eq).strip();
            String v = part.substring(eq + 1).strip();
            if (!k.isEmpty()) {
                m.put(k, v);
            }
        }
        return m;
    }

    /** 从 Set-Cookie 原始值（含 Path/HttpOnly 等属性）中提取首个 name=value。 */
    private static void mergeSetCookies(Map<String, String> dst, List<String> setCookies) {
        for (String raw : setCookies) {
            // 取第一个分号前的 name=value
            String first = raw.split(";")[0].strip();
            int eq = first.indexOf('=');
            if (eq <= 0) {
                continue;
            }
            String k = first.substring(0, eq).strip();
            String v = first.substring(eq + 1).strip();
            if (!k.isEmpty()) {
                dst.put(k, v);
            }
        }
    }

    /** 将 cookie 映射序列化为 Cookie 请求头值。 */
    private static String buildCookieHdr(Map<String, String> m) {
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> e : m.entrySet()) {
            if (sb.length() > 0) {
                sb.append("; ");
            }
            sb.append(e.getKey()).append('=').append(e.getValue());
        }
        return sb.toString();
    }

    // ── Token 编解码 ─────────────────────────────────────────────────────────

    private static String encodeToken(String locale, String cookieHdr) {
        String json = "{\"l\":" + jsonStr(locale) + ",\"c\":" + jsonStr(cookieHdr) + "}";
        String b64 = Base64.getEncoder().encodeToString(json.getBytes(StandardCharsets.UTF_8));
        return TOK_PREFIX + b64;
    }

    private static String[] decodeToken(String token) {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw new RuntimeException("moakt: invalid session token");
        }
        try {
            byte[] raw = Base64.getDecoder().decode(token.substring(TOK_PREFIX.length()));
            String json = new String(raw, StandardCharsets.UTF_8);
            // 简单提取 "l" 和 "c" 字段（避免引入额外 JSON 依赖）
            String locale = extractJsonStr(json, "l");
            String cookieHdr = extractJsonStr(json, "c");
            if (locale == null || locale.isBlank() || cookieHdr == null || cookieHdr.isBlank()) {
                throw new RuntimeException("moakt: invalid session token");
            }
            return new String[]{locale, cookieHdr};
        } catch (RuntimeException e) {
            throw e;
        } catch (Exception e) {
            throw new RuntimeException("moakt: invalid session token", e);
        }
    }

    /** 最小化 JSON 字符串序列化（转义双引号和反斜杠）。 */
    private static String jsonStr(String s) {
        return "\"" + s.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
    }

    /** 从简单 JSON 对象中提取字符串字段值（不处理嵌套）。 */
    private static String extractJsonStr(String json, String key) {
        // 匹配 "key":"value"，value 中允许转义字符
        Pattern p = Pattern.compile("\"" + Pattern.quote(key) + "\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"");
        Matcher m = p.matcher(json);
        if (!m.find()) {
            return null;
        }
        return m.group(1).replace("\\\"", "\"").replace("\\\\", "\\");
    }

    // ── 请求头构造 ───────────────────────────────────────────────────────────

    private static Map<String, String> pageHeaders(String referer, String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        h.put("Cache-Control", "no-cache");
        h.put("DNT", "1");
        h.put("Pragma", "no-cache");
        h.put("Upgrade-Insecure-Requests", "1");
        h.put("User-Agent", DEFAULT_UA);
        h.put("Referer", referer);
        if (cookie != null && !cookie.isBlank()) {
            h.put("Cookie", cookie);
        }
        return h;
    }

    // ── HTML 解析工具 ────────────────────────────────────────────────────────

    private static String stripTags(String s) {
        return TAG_RE.matcher(s).replaceAll(" ").strip();
    }

    private static String unescapeHtml(String s) {
        return s.replace("&amp;", "&")
                .replace("&lt;", "<")
                .replace("&gt;", ">")
                .replace("&quot;", "\"")
                .replace("&#39;", "'")
                .replace("&nbsp;", " ");
    }

    private static String parseInboxEmail(String html) {
        Matcher m = EMAIL_DIV_RE.matcher(html);
        if (!m.find()) {
            throw new RuntimeException("moakt: email-address not found");
        }
        String addr = unescapeHtml(m.group(1).strip());
        if (addr.isEmpty()) {
            throw new RuntimeException("moakt: empty email-address");
        }
        return addr;
    }

    private static List<String> listMailIds(String html) {
        List<String> out = new ArrayList<>();
        java.util.Set<String> seen = new java.util.LinkedHashSet<>();
        Matcher m = HREF_EMAIL_RE.matcher(html);
        while (m.find()) {
            String path = m.group(1);
            if (path.contains("/delete")) {
                continue;
            }
            String mid = path.substring(path.lastIndexOf('/') + 1);
            if (mid.length() == 36 && !seen.contains(mid)) {
                seen.add(mid);
                out.add(mid);
            }
        }
        return out;
    }

    private static Map<String, Object> parseDetail(String page, String mid, String recipient) {
        String fromS = "";
        Matcher sm = SENDER_RE.matcher(page);
        if (sm.find()) {
            String inner = unescapeHtml(sm.group(1));
            fromS = stripTags(inner);
            Matcher em = FROM_ADDR_RE.matcher(inner);
            if (em.find()) {
                fromS = em.group(1).strip();
            }
        }
        String subj = "";
        Matcher tm = TITLE_RE.matcher(page);
        if (tm.find()) {
            subj = unescapeHtml(tm.group(1).strip());
        }
        String dateS = "";
        Matcher dm = DATE_RE.matcher(page);
        if (dm.find()) {
            dateS = unescapeHtml(dm.group(1).strip());
        }
        String body = extractBodyHtml(page);
        Map<String, Object> raw = new LinkedHashMap<>();
        raw.put("id", mid);
        raw.put("to", recipient);
        raw.put("from", fromS);
        raw.put("subject", subj);
        raw.put("date", dateS);
        raw.put("html", body);
        return raw;
    }

    // ── 域名解析 ─────────────────────────────────────────────────────────────

    /**
     * 解析 domain 参数：若为合法邮件域名则返回 ["zh", domain]，否则视为 locale 返回 [domain, ""]。
     */
    private static String[] requestParts(String domain) {
        String s = (domain == null ? "" : domain).strip();
        if (s.isEmpty() || s.contains("/") || s.contains("?") || s.contains("#") || s.contains("\\")) {
            return new String[]{"zh", ""};
        }
        if (MAIL_DOMAIN_RE.matcher(s).matches()) {
            return new String[]{"zh", s.replaceFirst("^@", "").toLowerCase()};
        }
        return new String[]{s, ""};
    }

    private static java.util.Set<String> parseServerDomains(String page) {
        java.util.Set<String> set = new java.util.LinkedHashSet<>();
        Matcher m = DOMAIN_OPTION_RE.matcher(page);
        while (m.find()) {
            String v = m.group(1).strip().replaceFirst("^@", "").toLowerCase();
            if (!v.isEmpty()) {
                set.add(v);
            }
        }
        return set;
    }

    private static String emailDomain(String email) {
        int at = email.lastIndexOf('@');
        return at >= 0 ? email.substring(at + 1).strip().toLowerCase() : "";
    }

    // ── 公开 API ─────────────────────────────────────────────────────────────

    /**
     * 创建临时邮箱。
     *
     * @param channel 渠道标识
     * @param domain  指定域名或 locale，可为 null
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel, String domain) {
        String[] parts = requestParts(domain);
        String locale = parts[0];
        String mailDomain = parts[1];

        String base = ORIGIN + "/" + locale;
        String inbox = base + "/inbox";

        // 第一步：GET 首页，获取初始 Cookie
        HttpResult r1 = HttpClient.get(base, pageHeaders(base, null));
        r1.ensureSuccess();
        Map<String, String> cookieMap = parseCookieMap("");
        mergeSetCookies(cookieMap, r1.getSetCookies());
        String cookieHdr = buildCookieHdr(cookieMap);

        // 验证指定域名是否可用
        if (!mailDomain.isEmpty()) {
            java.util.Set<String> serverDomains = parseServerDomains(r1.getBody());
            if (!serverDomains.contains(mailDomain)) {
                throw new RuntimeException("moakt: unsupported domain " + mailDomain);
            }
        }

        // 第二步：POST /inbox 创建邮箱（不跟随重定向，捕获 302 的 Set-Cookie）
        String postBody;
        if (!mailDomain.isEmpty()) {
            postBody = "setemail=&username=" + ProviderUtil.urlEncode(ProviderUtil.randomString(12))
                    + "&domain=" + ProviderUtil.urlEncode(mailDomain) + "&preferred_domain=";
        } else {
            postBody = "random=1";
        }
        Map<String, String> postHeaders = pageHeaders(base, cookieHdr);
        postHeaders.put("Content-Type", "application/x-www-form-urlencoded");
        HttpResult r2 = HttpClient.postNoRedirect(inbox, postBody, "application/x-www-form-urlencoded", postHeaders);
        mergeSetCookies(cookieMap, r2.getSetCookies());
        cookieHdr = buildCookieHdr(cookieMap);

        if (!cookieMap.containsKey("tm_session")) {
            throw new RuntimeException("moakt: missing tm_session cookie");
        }

        // 第三步：GET /inbox 获取邮箱地址
        HttpResult r3 = HttpClient.get(inbox, pageHeaders(base, cookieHdr));
        r3.ensureSuccess();
        mergeSetCookies(cookieMap, r3.getSetCookies());
        cookieHdr = buildCookieHdr(cookieMap);

        String email = parseInboxEmail(r3.getBody());
        if (!mailDomain.isEmpty() && !emailDomain(email).equals(mailDomain)) {
            throw new RuntimeException("moakt: domain mismatch expected=" + mailDomain
                    + " actual=" + emailDomain(email));
        }

        String token = encodeToken(locale, cookieHdr);
        return new EmailInfo(channel, email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 会话令牌
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String[] parts = decodeToken(token);
        String locale = parts[0];
        String cookieHdr = parts[1];

        String base = ORIGIN + "/" + locale;
        String inbox = base + "/inbox";

        HttpResult r = HttpClient.get(inbox, pageHeaders(base, cookieHdr));
        r.ensureSuccess();

        List<String> ids = listMailIds(r.getBody());
        List<Email> out = new ArrayList<>();
        for (String mid : ids) {
            String detailUrl = ORIGIN + "/" + locale + "/email/" + mid + "/html";
            String refer = ORIGIN + "/" + locale + "/email/" + mid;
            try {
                HttpResult rd = HttpClient.get(detailUrl, pageHeaders(refer, cookieHdr));
                if (!rd.isOk()) {
                    continue;
                }
                Map<String, Object> raw = parseDetail(rd.getBody(), mid, email);
                out.add(Normalizer.normalizeEmail(raw, email));
            } catch (RuntimeException ignored) {
                // 跳过单封解析失败
            }
        }
        return out;
    }
}
