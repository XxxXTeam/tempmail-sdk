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
 * MailCatch 渠道 — https://mailcatch.com
 * 公开收件箱，无需认证。随机用户名 + @mailcatch.com。
 * 获取邮件: GET /api/list/{inbox} + GET /api/data/{inbox}/{id}。
 */
public final class Mailcatch {

    private static final String CHANNEL = "mailcatch";
    private static final String BASE_URL = "https://mailcatch.com";
    private static final String DOMAIN = "mailcatch.com";

    /** 邮件列表 HTML 中提取邮件项 */
    private static final Pattern EMAIL_ITEM_RE = Pattern.compile(
            "class=\"email-item\"\\s+"
                    + "data-email-id=\"([^\"]*)\"\\s+"
                    + "data-subject=\"([^\"]*)\"\\s+"
                    + "data-timestamp=\"([^\"]*)\"\\s+"
                    + "data-sender=\"([^\"]*)\"",
            Pattern.CASE_INSENSITIVE | Pattern.DOTALL);

    private Mailcatch() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        return h;
    }

    /**
     * 创建 mailcatch 临时邮箱（公开收件箱，无需 API 调用）。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String local = ProviderUtil.randomLocalSdk();
        String email = local + "@" + DOMAIN;
        return new EmailInfo(CHANNEL, email, local, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 收件箱名称（用户名）
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (token == null || token.isBlank()) {
            throw new IllegalArgumentException("mailcatch: token 不能为空");
        }
        String addr = (email != null) ? email.trim() : "";
        if (addr.isEmpty()) {
            throw new IllegalArgumentException("mailcatch: 邮箱地址不能为空");
        }

        String inbox = token.trim();
        if (inbox.isEmpty()) {
            int at = addr.indexOf('@');
            inbox = at > 0 ? addr.substring(0, at) : addr;
        }

        // 获取邮件列表 HTML
        HttpResult resp = HttpClient.get(BASE_URL + "/api/list/" + ProviderUtil.urlEncode(inbox), headers());
        resp.ensureSuccess();

        Matcher matcher = EMAIL_ITEM_RE.matcher(resp.getBody());
        List<Email> emails = new ArrayList<>();

        while (matcher.find()) {
            String emailId = matcher.group(1).trim();
            String subject = matcher.group(2).trim();
            String timestamp = matcher.group(3).trim();
            String sender = matcher.group(4).trim();

            if (emailId.isEmpty()) continue;

            // 获取邮件正文
            String bodyHtml = "";
            try {
                HttpResult detail = HttpClient.get(
                        BASE_URL + "/api/data/" + ProviderUtil.urlEncode(inbox) + "/" + ProviderUtil.urlEncode(emailId),
                        headers());
                if (detail.isOk()) {
                    bodyHtml = detail.getBody().trim();
                }
            } catch (RuntimeException ignored) {
            }

            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", emailId);
            raw.put("from", sender);
            raw.put("to", addr);
            raw.put("subject", subject);
            raw.put("html", bodyHtml);
            raw.put("date", timestamp);
            emails.add(Normalizer.normalizeEmail(raw, addr));
        }
        return emails;
    }
}
