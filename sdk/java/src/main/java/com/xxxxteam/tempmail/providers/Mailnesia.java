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
 * Mailnesia 渠道 — https://mailnesia.com
 * HTML 解析模式，公开收件箱，无需认证。
 */
public final class Mailnesia {

    private static final String CHANNEL = "mailnesia";
    private static final String BASE_URL = "https://mailnesia.com";
    private static final String DOMAIN = "mailnesia.com";

    private static final Pattern ROW_RE = Pattern.compile(
            "<tr\\s+id=\"([^\"]+)\"[^>]*class=\"[^\"]*\\bemailheader\\b[^\"]*\"[^>]*>(.*?)</tr>",
            Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
    private static final Pattern TIME_RE = Pattern.compile(
            "<time\\s+datetime=\"([^\"]+)\"", Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
    private static final Pattern ANCHOR_RE = Pattern.compile(
            "<a\\b[^>]*class=\"email\"[^>]*>(.*?)</a>", Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
    private static final Pattern TAG_RE = Pattern.compile("<[^>]+>", Pattern.DOTALL);
    private static final Pattern PLAIN_RE = Pattern.compile(
            "<div\\s+id=\"text_plain_([^\"]+)\"[^>]*>\\s*<pre>(.*?)</pre>\\s*</div>",
            Pattern.CASE_INSENSITIVE | Pattern.DOTALL);

    private Mailnesia() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html,*/*");
        return h;
    }

    private static String cleanText(String raw) {
        if (raw == null) return "";
        String noTags = TAG_RE.matcher(raw).replaceAll(" ");
        return noTags.replaceAll("&nbsp;", " ").replaceAll("&amp;", "&")
                .replaceAll("&lt;", "<").replaceAll("&gt;", ">")
                .replaceAll("\\s+", " ").trim();
    }

    /**
     * 生成随机邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String local = ProviderUtil.randomLocalSdk();
        // 访问一次邮箱页面以"激活"
        try {
            HttpClient.get(BASE_URL + "/mailbox/" + ProviderUtil.urlEncode(local), headers());
        } catch (RuntimeException ignored) {
        }
        return new EmailInfo(CHANNEL, local + "@" + DOMAIN);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        String local = localPart(email);
        if (local.isEmpty()) {
            throw new IllegalArgumentException("mailnesia: 邮箱地址为空");
        }

        HttpResult resp = HttpClient.get(BASE_URL + "/mailbox/" + ProviderUtil.urlEncode(local), headers());
        resp.ensureSuccess();
        String page = resp.getBody();

        List<Map<String, String>> rows = parseRows(page);
        List<Email> emails = new ArrayList<>();
        for (Map<String, String> row : rows) {
            Map<String, Object> detail = fetchDetail(local, row, page);
            try {
                emails.add(Normalizer.normalizeEmail(detail, email));
            } catch (RuntimeException e) {
                emails.add(Normalizer.normalizeEmail(new LinkedHashMap<>(row), email));
            }
        }
        return emails;
    }

    private static String localPart(String email) {
        if (email == null || email.isBlank()) return "";
        String trimmed = email.trim();
        int at = trimmed.indexOf('@');
        return at > 0 ? trimmed.substring(0, at) : trimmed;
    }

    private static List<Map<String, String>> parseRows(String page) {
        List<Map<String, String>> rows = new ArrayList<>();
        Matcher rowMatcher = ROW_RE.matcher(page);
        while (rowMatcher.find()) {
            String messageId = rowMatcher.group(1).trim();
            String rowHtml = rowMatcher.group(2);

            Matcher dateMatcher = TIME_RE.matcher(rowHtml);
            String date = dateMatcher.find() ? dateMatcher.group(1).trim() : "";

            List<String> anchors = new ArrayList<>();
            Matcher anchorMatcher = ANCHOR_RE.matcher(rowHtml);
            while (anchorMatcher.find()) {
                anchors.add(cleanText(anchorMatcher.group(1)));
            }
            if (anchors.size() < 3) continue;

            Map<String, String> row = new LinkedHashMap<>();
            row.put("id", messageId);
            row.put("date", date);
            row.put("from", anchors.get(0));
            row.put("to", anchors.get(1));
            row.put("subject", anchors.get(2));
            rows.add(row);
        }
        return rows;
    }

    private static Map<String, Object> fetchDetail(String local, Map<String, String> row, String listPage) {
        String messageId = row.get("id");
        Map<String, Object> detail = new LinkedHashMap<>(row);
        if (messageId == null || messageId.isEmpty()) return detail;

        try {
            HttpResult resp = HttpClient.get(
                    BASE_URL + "/mailbox/" + ProviderUtil.urlEncode(local) + "/" + ProviderUtil.urlEncode(messageId),
                    headers());
            if (!resp.isOk()) return detail;
            String detailPage = resp.getBody();

            // 提取纯文本
            Pattern plainPattern = Pattern.compile(
                    "<div\\s+id=\"text_plain_" + Pattern.quote(messageId) + "\"[^>]*>\\s*<pre>(.*?)</pre>\\s*</div>",
                    Pattern.CASE_INSENSITIVE | Pattern.DOTALL);
            Matcher plainMatcher = plainPattern.matcher(detailPage);
            if (plainMatcher.find()) {
                detail.put("text", plainMatcher.group(1).replaceAll("&lt;", "<")
                        .replaceAll("&gt;", ">").replaceAll("&amp;", "&").trim());
            }

            // 提取 HTML 正文
            String htmlDivId = "text_html_" + messageId;
            int pos = detailPage.indexOf("id=\"" + htmlDivId + "\"");
            if (pos >= 0) {
                int openEnd = detailPage.indexOf(">", pos);
                if (openEnd >= 0) {
                    int start = openEnd + 1;
                    String plainDivId = "text_plain_" + messageId;
                    int end = detailPage.indexOf("<div id=\"" + plainDivId + "\"", start);
                    if (end < 0) {
                        end = detailPage.indexOf("</div>", start);
                        if (end >= 0) end += "</div>".length();
                    }
                    if (end > start) {
                        String htmlContent = detailPage.substring(start, end).trim();
                        if (htmlContent.endsWith("</div>")) {
                            htmlContent = htmlContent.substring(0, htmlContent.length() - "</div>".length()).trim();
                        }
                        detail.put("html", htmlContent);
                    }
                }
            }
        } catch (RuntimeException ignored) {
        }
        return detail;
    }
}
