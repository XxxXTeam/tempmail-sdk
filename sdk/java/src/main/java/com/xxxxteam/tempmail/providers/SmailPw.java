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
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * smail.pw 渠道 — https://smail.pw
 * POST _root.data 创建邮箱，GET _root.data 获取邮件列表，
 * GET /api/email/{id} 获取单封邮件正文。
 */
public final class SmailPw {

    private static final String CHANNEL = "smail-pw";
    private static final String ROOT_DATA_URL = "https://smail.pw/_root.data";

    private static final Pattern QUOTED_INBOX = Pattern.compile("\"([a-z0-9][a-z0-9.\\-]*@smail\\.pw)\"", Pattern.CASE_INSENSITIVE);
    private static final Pattern PLAIN_INBOX = Pattern.compile("\\b([a-z0-9][a-z0-9.\\-]*@smail\\.pw)\\b", Pattern.CASE_INSENSITIVE);
    private static final Pattern MAIL_RE = Pattern.compile(
            "\"id\",\"([^\"]+)\",\"to_address\",\"([^\"]*)\",\"from_name\",\"([^\"]*)\",\"from_address\",\"([^\"]*)\",\"subject\",\"([^\"]*)\",\"time\",(\\d+)");
    private static final Pattern MAIL2_RE = Pattern.compile(
            "\"id\",\"([^\"]+)\",\"from_name\",\"([^\"]*)\",\"from_address\",\"([^\"]*)\",\"subject\",\"([^\"]*)\",\"time\",(\\d+)");

    private SmailPw() {
    }

    private static Map<String, String> defaultHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "*/*");
        h.put("accept-language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6");
        h.put("cache-control", "no-cache");
        h.put("dnt", "1");
        h.put("origin", "https://smail.pw");
        h.put("pragma", "no-cache");
        h.put("referer", "https://smail.pw/");
        h.put("sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"");
        h.put("sec-ch-ua-mobile", "?0");
        h.put("sec-ch-ua-platform", "\"Windows\"");
        h.put("sec-fetch-dest", "empty");
        h.put("sec-fetch-mode", "cors");
        h.put("sec-fetch-site", "same-origin");
        return h;
    }

    private static String extractSessionCookie(HttpResult resp) {
        for (String sc : resp.getSetCookies()) {
            if (sc.contains("__session=")) {
                int start = sc.indexOf("__session=") + "__session=".length();
                int end = sc.indexOf(';', start);
                String val = end > 0 ? sc.substring(start, end) : sc.substring(start);
                return "__session=" + val;
            }
        }
        // 尝试从 Set-Cookie 头中提取
        String scHeader = resp.getHeaders().get("Set-Cookie");
        if (scHeader != null) {
            Matcher m = Pattern.compile("__session=([^;]+)").matcher(scHeader);
            if (m.find()) return "__session=" + m.group(1);
        }
        return "";
    }

    private static String extractInbox(String text) {
        Matcher m = QUOTED_INBOX.matcher(text);
        if (m.find()) return m.group(1);
        m = PLAIN_INBOX.matcher(text);
        return m.find() ? m.group(1) : "";
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> h = defaultHeaders();
        h.put("Content-Type", "application/x-www-form-urlencoded;charset=UTF-8");
        HttpResult resp = HttpClient.post(ROOT_DATA_URL, "intent=generate",
                "application/x-www-form-urlencoded;charset=UTF-8", h);
        resp.ensureSuccess();

        String cookie = extractSessionCookie(resp);
        if (cookie.isEmpty()) {
            throw new RuntimeException("smail-pw: 未提取到 __session cookie");
        }

        String email = extractInbox(resp.getBody());
        if (email.isEmpty()) {
            throw new RuntimeException("smail-pw: 无法从响应中解析邮箱地址");
        }
        return new EmailInfo(CHANNEL, email, cookie, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token  __session cookie
     * @param email  邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        if (token == null || token.isBlank()) {
            throw new IllegalArgumentException("smail-pw: token 不能为空");
        }
        Map<String, String> h = defaultHeaders();
        h.put("Cookie", token);
        HttpResult resp = HttpClient.get(ROOT_DATA_URL, h);
        resp.ensureSuccess();

        List<Map<String, Object>> rawList = parseMails(resp.getBody(), email);
        List<Email> out = new ArrayList<>();
        for (Map<String, Object> raw : rawList) {
            Email em = Normalizer.normalizeEmail(raw, email);
            // 尝试获取正文
            if (em.getId() != null && !em.getId().isEmpty() && !em.getId().startsWith("__smail_")) {
                Map<String, String> body = fetchEmailBody(token, em.getId());
                if (!body.get("html").isEmpty()) em.setHtml(body.get("html"));
                if (!body.get("text").isEmpty()) em.setText(body.get("text"));
            }
            out.add(em);
        }
        return out;
    }

    private static Map<String, String> fetchEmailBody(String token, String mailId) {
        Map<String, String> result = new LinkedHashMap<>();
        result.put("text", "");
        result.put("html", "");
        try {
            Map<String, String> h = defaultHeaders();
            h.put("Cookie", token);
            h.put("Accept", "application/json");
            HttpResult resp = HttpClient.get("https://smail.pw/api/email/" + ProviderUtil.urlEncode(mailId), h);
            if (!resp.isOk()) return result;
            JsonElement data = Json.parse(resp.getBody());
            if (data != null && data.isJsonObject()) {
                String html = Json.str(data, "body");
                result.put("html", html);
            }
        } catch (RuntimeException ignored) {
        }
        return result;
    }

    private static List<Map<String, Object>> parseMails(String text, String recipient) {
        Set<String> seenIds = new LinkedHashSet<>();
        List<Map<String, Object>> all = new ArrayList<>();

        // 正则匹配模式1
        Matcher m1 = MAIL_RE.matcher(text);
        while (m1.find()) {
            Map<String, Object> mail = new LinkedHashMap<>();
            mail.put("id", m1.group(1));
            mail.put("to_address", m1.group(2).isEmpty() ? recipient : m1.group(2));
            mail.put("from_name", m1.group(3));
            mail.put("from_address", m1.group(4));
            mail.put("subject", m1.group(5));
            mail.put("date", Long.parseLong(m1.group(6)));
            mail.put("text", "");
            mail.put("html", "");
            String id = m1.group(1);
            if (!seenIds.contains(id)) {
                seenIds.add(id);
                all.add(mail);
            }
        }

        // 正则匹配模式2
        Matcher m2 = MAIL2_RE.matcher(text);
        while (m2.find()) {
            Map<String, Object> mail = new LinkedHashMap<>();
            mail.put("id", m2.group(1));
            mail.put("to_address", recipient);
            mail.put("from_name", m2.group(2));
            mail.put("from_address", m2.group(3));
            mail.put("subject", m2.group(4));
            mail.put("date", Long.parseLong(m2.group(5)));
            mail.put("text", "");
            mail.put("html", "");
            String id = m2.group(1);
            if (!seenIds.contains(id)) {
                seenIds.add(id);
                all.add(mail);
            }
        }

        // 尝试 JSON 解析
        try {
            JsonElement root = Json.parse(text);
            if (root != null) {
                walkJsonEmails(root, recipient, all, seenIds);
            }
        } catch (RuntimeException ignored) {
        }

        return all;
    }

    private static void walkJsonEmails(JsonElement node, String recipient,
                                       List<Map<String, Object>> out, Set<String> seenIds) {
        if (node == null) return;
        if (node.isJsonArray()) {
            for (JsonElement el : node.getAsJsonArray()) {
                walkJsonEmails(el, recipient, out, seenIds);
            }
            return;
        }
        if (!node.isJsonObject()) return;
        com.google.gson.JsonObject obj = node.getAsJsonObject();

        // 检查是否为邮件对象（有 subject 和 time 字段）
        if (obj.has("subject") && obj.get("subject").isJsonPrimitive()
                && obj.has("time")) {
            String id = Json.str(obj, "id");
            String subject = Json.str(obj, "subject");
            String timeStr = Json.str(obj, "time");
            long timeVal;
            try {
                timeVal = Long.parseLong(timeStr);
            } catch (NumberFormatException e) {
                return;
            }

            // 检查 subject 不是地址本身
            if (subject.equals(recipient) || subject.endsWith("@smail.pw")) {
                subject = "";
            }

            if (!seenIds.contains(id) && !id.isEmpty()) {
                seenIds.add(id);
                Map<String, Object> mail = new LinkedHashMap<>();
                mail.put("id", id);
                mail.put("to_address", Json.str(obj, "to_address").isEmpty() ? recipient : Json.str(obj, "to_address"));
                mail.put("from_name", Json.str(obj, "from_name"));
                mail.put("from_address", Json.str(obj, "from_address"));
                mail.put("subject", subject);
                mail.put("date", timeVal);
                mail.put("text", "");
                mail.put("html", "");
                out.add(mail);
            }
        }

        // 递归子对象
        for (Map.Entry<String, JsonElement> entry : obj.entrySet()) {
            JsonElement v = entry.getValue();
            if (v.isJsonObject() || v.isJsonArray()) {
                walkJsonEmails(v, recipient, out, seenIds);
            }
        }
    }
}
