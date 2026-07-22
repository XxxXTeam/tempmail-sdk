package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * tempmailten.com 渠道 — https://tempmailten.com
 * Laravel CSRF + session，POST /messages 获取邮箱和邮件列表，GET /view/{id} 获取正文。
 * token 使用 base64 编码，前缀 "tmt1:"。
 */
public final class TempMailTen {

    private static final String BASE_URL = "https://tempmailten.com";
    private static final String TOK_PREFIX = "tmt1:";
    private static final String UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private TempMailTen() {
    }

    private static Map<String, String> ajaxHeaders(String csrf, String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("Accept-Language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("X-CSRF-TOKEN", csrf);
        h.put("Content-Type", "application/x-www-form-urlencoded");
        h.put("Referer", BASE_URL + "/en");
        h.put("Cookie", cookie);
        return h;
    }

    private static String encodeToken(String csrf, String cookie) {
        String raw = "{\"t\":\"" + csrf + "\",\"c\":\"" + cookie.replace("\"", "\\\"") + "\"}";
        return TOK_PREFIX + ProviderUtil.base64Encode(raw.getBytes(StandardCharsets.UTF_8));
    }

    private static String[] decodeToken(String token) {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw new RuntimeException("tempmailten: 无效的 token");
        }
        byte[] decoded = ProviderUtil.base64Decode(token.substring(TOK_PREFIX.length()));
        JsonElement obj = Json.parse(new String(decoded, StandardCharsets.UTF_8));
        String csrf = Json.str(obj, "t").trim();
        String cookie = Json.str(obj, "c").trim();
        if (csrf.isEmpty()) {
            throw new RuntimeException("tempmailten: token 中缺少 CSRF");
        }
        return new String[]{csrf, cookie};
    }

    private static JsonElement postMessages(String csrf, String cookie) {
        String body = "_token=" + ProviderUtil.urlEncode(csrf) + "&captcha=";
        HttpResult resp = HttpClient.post(BASE_URL + "/messages", body,
                "application/x-www-form-urlencoded", ajaxHeaders(csrf, cookie));
        resp.ensureSuccess();
        return Json.parse(resp.getBody());
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        Map<String, String> pageH = new LinkedHashMap<>();
        pageH.put("User-Agent", UA);
        pageH.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        HttpResult resp = HttpClient.get(BASE_URL + "/en", pageH);
        resp.ensureSuccess();
        String cookie = ProviderUtil.extractAllCookies(resp);
        String csrf = ProviderUtil.extractCsrfToken(resp.getBody());
        if (csrf.isEmpty()) {
            throw new RuntimeException("tempmailten: 未能从首页提取 CSRF token");
        }
        JsonElement data = postMessages(csrf, cookie);
        String mailbox = Json.str(data, "mailbox").trim();
        if (mailbox.isEmpty() || !mailbox.contains("@")) {
            throw new RuntimeException("tempmailten: 邮箱地址无效");
        }
        return new EmailInfo("tempmailten", mailbox, encodeToken(csrf, cookie), null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token base64 编码的认证信息
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("tempmailten: 邮箱地址为空");
        }
        String[] parts = decodeToken(token);
        String csrf = parts[0];
        String cookie = parts[1];
        JsonElement data = postMessages(csrf, cookie);
        JsonArray msgs = Json.arr(data, "messages");
        if (msgs == null || msgs.size() == 0) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : msgs) {
            if (item == null || !item.isJsonObject()) continue;
            String mid = Json.str(item, "id").trim();
            if (mid.isEmpty()) continue;
            String fromAddr = Json.str(item, "from_email");
            String fromName = Json.str(item, "from");
            if (!fromName.isEmpty() && !fromName.equals(fromAddr)) {
                fromAddr = fromName + " <" + fromAddr + ">";
            }
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", mid);
            raw.put("from", fromAddr);
            raw.put("to", address);
            raw.put("subject", Json.str(item, "subject"));
            raw.put("html", fetchView(cookie, mid));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static String fetchView(String cookie, String mid) {
        if (mid == null || mid.isEmpty()) return "";
        try {
            Map<String, String> h = new LinkedHashMap<>();
            h.put("User-Agent", UA);
            h.put("Accept", "text/html,*/*");
            h.put("Referer", BASE_URL + "/en");
            h.put("Cookie", cookie);
            HttpResult resp = HttpClient.get(BASE_URL + "/view/" + ProviderUtil.urlEncode(mid), h);
            return resp.isOk() ? resp.getBody() : "";
        } catch (Exception e) {
            return "";
        }
    }
}
