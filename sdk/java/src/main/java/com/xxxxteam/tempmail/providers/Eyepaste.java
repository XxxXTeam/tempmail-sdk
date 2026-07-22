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
 * Eyepaste 渠道 — https://eyepaste.com
 * RSS XML API，无认证，直接生成随机用户名。
 */
public final class Eyepaste {

    private static final String CHANNEL = "eyepaste";
    private static final String DOMAIN = "eyepaste.com";
    private static final String BASE = "https://www.eyepaste.com";

    /** RSS description 中提取元数据的正则 */
    private static final Pattern P_TAG = Pattern.compile("<p[^>]*>(.*?)</p>", Pattern.DOTALL | Pattern.CASE_INSENSITIVE);
    private static final Pattern FROM_RE = Pattern.compile("From:\\s*(.*?)(?:\\s+To:|$)", Pattern.DOTALL);
    private static final Pattern TO_RE = Pattern.compile("To:\\s*(.*?)(?:\\s+Subject:|$)", Pattern.DOTALL);
    private static final Pattern SUBJECT_RE = Pattern.compile("Subject:\\s*(.*?)(?:\\s+Date:|$)", Pattern.DOTALL);
    private static final Pattern DATE_RE = Pattern.compile("Date:\\s*(.*?)$", Pattern.DOTALL);
    private static final Pattern ITEM_RE = Pattern.compile("<item>(.*?)</item>", Pattern.DOTALL);
    private static final Pattern TITLE_RE = Pattern.compile("<title>(.*?)</title>", Pattern.DOTALL);
    private static final Pattern DESC_RE = Pattern.compile("<description>(.*?)</description>", Pattern.DOTALL);
    private static final Pattern TAG_RE = Pattern.compile("<[^>]+>");

    private Eyepaste() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/rss+xml, application/xml, text/xml, */*");
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        return h;
    }

    /**
     * 生成随机邮箱地址，无需 API 调用。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String username = ProviderUtil.randomString(10);
        String email = username + "@" + DOMAIN;
        return new EmailInfo(CHANNEL, email);
    }

    /**
     * 获取邮件列表，通过 RSS XML 解析。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.isBlank()) {
            throw new IllegalArgumentException("eyepaste: 邮箱地址为空");
        }
        String e = email.trim();
        HttpResult resp = HttpClient.get(BASE + "/inbox/" + ProviderUtil.urlEncode(e) + ".rss", headers());
        resp.ensureSuccess();

        String content = resp.getBody();
        if (content == null || content.isBlank()) {
            return new ArrayList<>();
        }

        List<Email> emails = new ArrayList<>();
        Matcher itemMatcher = ITEM_RE.matcher(content);
        int idx = 0;
        while (itemMatcher.find()) {
            idx++;
            String itemXml = itemMatcher.group(1);
            String title = extractTag(TITLE_RE, itemXml);
            String desc = extractTag(DESC_RE, itemXml);

            Map<String, String> parsed = parseDescription(desc);
            String subject = parsed.get("subject").isEmpty() ? title : parsed.get("subject");
            String fromAddr = parsed.get("from");
            String toAddr = parsed.get("to").isEmpty() ? e : parsed.get("to");
            String date = parsed.get("date");
            String bodyHtml = parsed.get("body");
            String text = bodyHtml.isEmpty() ? "" : TAG_RE.matcher(bodyHtml).replaceAll("").trim();

            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", String.valueOf(idx));
            raw.put("from", fromAddr);
            raw.put("to", toAddr);
            raw.put("subject", subject);
            raw.put("text", text);
            raw.put("html", bodyHtml);
            raw.put("date", date);
            emails.add(Normalizer.normalizeEmail(raw, e));
        }
        return emails;
    }

    private static String extractTag(Pattern pattern, String xml) {
        Matcher m = pattern.matcher(xml);
        return m.find() ? m.group(1).trim() : "";
    }

    private static Map<String, String> parseDescription(String desc) {
        Map<String, String> result = new LinkedHashMap<>();
        result.put("from", "");
        result.put("to", "");
        result.put("subject", "");
        result.put("date", "");
        result.put("body", "");

        if (desc == null || desc.isEmpty()) {
            return result;
        }

        Matcher pMatch = P_TAG.matcher(desc);
        if (pMatch.find()) {
            String meta = pMatch.group(1).trim();
            Matcher fm = FROM_RE.matcher(meta);
            if (fm.find()) result.put("from", fm.group(1).trim());
            Matcher tm = TO_RE.matcher(meta);
            if (tm.find()) result.put("to", tm.group(1).trim());
            Matcher sm = SUBJECT_RE.matcher(meta);
            if (sm.find()) result.put("subject", sm.group(1).trim());
            Matcher dm = DATE_RE.matcher(meta);
            if (dm.find()) result.put("date", dm.group(1).trim());

            int pEnd = desc.indexOf("</p>");
            if (pEnd != -1) {
                String body = desc.substring(pEnd + 4).trim();
                if (!body.isEmpty()) {
                    result.put("body", body);
                }
            }
        }
        return result;
    }
}
