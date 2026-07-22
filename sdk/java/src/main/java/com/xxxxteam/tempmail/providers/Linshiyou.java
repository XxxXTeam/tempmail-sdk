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
 * linshiyou.com 渠道 — NEXUS_TOKEN cookie + HTML 分片解析。
 */
public final class Linshiyou {

    private static final String CHANNEL = "linshiyou";
    private static final String ORIGIN = "https://linshiyou.com";

    private static final Pattern LIST_ID_RE = Pattern.compile(
            "id=\"tmail-email-list-([a-f0-9]+)\"", Pattern.CASE_INSENSITIVE);
    private static final Pattern DIV_RE = Pattern.compile(
            "<div class=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</div>", Pattern.CASE_INSENSITIVE);
    private static final Pattern SRCDOC_RE = Pattern.compile(
            "\\ssrcdoc=\"([^\"]*)\"", Pattern.CASE_INSENSITIVE);
    private static final Pattern DOWNLOAD_RE = Pattern.compile(
            "href=\"(/api/download\\?id=[^\"]+)\"", Pattern.CASE_INSENSITIVE);
    private static final Pattern TAG_RE = Pattern.compile("<[^>]+>");

    private Linshiyou() {
    }

    private static Map<String, String> defaultHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        h.put("accept-language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6");
        h.put("cache-control", "no-cache");
        h.put("dnt", "1");
        h.put("pragma", "no-cache");
        h.put("priority", "u=1, i");
        h.put("referer", ORIGIN + "/");
        h.put("sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"");
        h.put("sec-ch-ua-mobile", "?0");
        h.put("sec-ch-ua-platform", "\"Windows\"");
        h.put("sec-fetch-dest", "empty");
        h.put("sec-fetch-mode", "cors");
        h.put("sec-fetch-site", "same-origin");
        return h;
    }

    private static String stripTags(String s) {
        if (s == null) return "";
        String noTags = TAG_RE.matcher(s).replaceAll(" ");
        return noTags.replaceAll("&amp;", "&").replaceAll("&lt;", "<")
                .replaceAll("&gt;", ">").replaceAll("&nbsp;", " ")
                .replaceAll("\\s+", " ").trim();
    }

    private static String pickDiv(String fragment, String className) {
        Matcher m = DIV_RE.matcher(fragment);
        while (m.find()) {
            if (m.group(1).equals(className)) {
                return stripTags(m.group(2)).trim();
            }
        }
        return "";
    }

    private static String extractSrcdoc(String bodyPart) {
        Matcher m = SRCDOC_RE.matcher(bodyPart);
        if (!m.find()) return "";
        return m.group(1).replaceAll("&amp;", "&").replaceAll("&lt;", "<")
                .replaceAll("&gt;", ">").replaceAll("&quot;", "\"");
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(ORIGIN + "/api/user?user", defaultHeaders());
        resp.ensureSuccess();

        // 从 Set-Cookie 提取 NEXUS_TOKEN
        String nexus = "";
        for (String sc : resp.getSetCookies()) {
            if (sc.contains("NEXUS_TOKEN=")) {
                int start = sc.indexOf("NEXUS_TOKEN=") + "NEXUS_TOKEN=".length();
                int end = sc.indexOf(';', start);
                nexus = end > 0 ? sc.substring(start, end) : sc.substring(start);
                break;
            }
        }
        if (nexus.isEmpty()) {
            throw new RuntimeException("linshiyou: NEXUS_TOKEN not in Set-Cookie");
        }

        String email = resp.getBody().trim();
        if (email.isEmpty() || !email.contains("@")) {
            throw new RuntimeException("linshiyou: invalid email in response body");
        }

        String token = "NEXUS_TOKEN=" + nexus + "; tmail-emails=" + email;
        return new EmailInfo(CHANNEL, email, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token cookie 字符串
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        Map<String, String> h = defaultHeaders();
        h.put("Cookie", token);
        h.put("x-requested-with", "XMLHttpRequest");
        HttpResult resp = HttpClient.get(ORIGIN + "/api/mail?unseen=1", h);
        resp.ensureSuccess();
        return parseSegments(resp.getBody(), email);
    }

    private static List<Email> parseSegments(String raw, String recipient) {
        if (raw == null || raw.isBlank()) return new ArrayList<>();
        List<Email> out = new ArrayList<>();

        String[] segments = raw.split("<-----TMAILNEXTMAIL----->");
        for (String seg : segments) {
            seg = seg.trim();
            if (seg.isEmpty()) continue;

            String[] parts = seg.split("<-----TMAILCHOPPER----->", 2);
            String listPart = parts[0];
            String bodyPart = parts.length > 1 ? parts[1] : "";

            Matcher midMatch = LIST_ID_RE.matcher(listPart);
            if (!midMatch.find()) continue;
            String mid = midMatch.group(1);

            String fromList = pickDiv(listPart, "name");
            String subjectList = pickDiv(listPart, "subject");
            String previewList = pickDiv(listPart, "body");

            String fromBody = pickDiv(bodyPart, "tmail-email-sender");
            String timeBody = pickDiv(bodyPart, "tmail-email-time");
            String titleBody = pickDiv(bodyPart, "tmail-email-title");
            String htmlBody = extractSrcdoc(bodyPart);

            String fromAddr = fromBody.isEmpty() ? fromList : fromBody;
            String subject = titleBody.isEmpty() ? subjectList : titleBody;
            String text = previewList.isEmpty() ? stripTags(htmlBody) : previewList;

            Map<String, Object> rawMap = new LinkedHashMap<>();
            rawMap.put("id", mid);
            rawMap.put("from", fromAddr);
            rawMap.put("to", recipient);
            rawMap.put("subject", subject);
            rawMap.put("text", text);
            rawMap.put("html", htmlBody);
            rawMap.put("date", timeBody);
            out.add(Normalizer.normalizeEmail(rawMap, recipient));
        }
        return out;
    }
}
