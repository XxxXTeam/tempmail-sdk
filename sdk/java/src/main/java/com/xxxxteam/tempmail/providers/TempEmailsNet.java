package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * tempemails.net 渠道 — https://tempemails.net
 * Laravel CSRF + session cookie，POST /get_messages 创建/获取邮件，GET /view/{id} 正文。
 */
public final class TempEmailsNet {

    private static final String BASE_URL = "https://tempemails.net";
    private static final String UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private TempEmailsNet() {
    }

    private static Map<String, String> pageHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        h.put("Accept-Language", "en-US,en;q=0.9");
        h.put("User-Agent", UA);
        return h;
    }

    private static Map<String, String> ajaxHeaders(String csrf, String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("X-CSRF-TOKEN", csrf);
        h.put("Cookie", cookie);
        h.put("Referer", BASE_URL + "/");
        h.put("User-Agent", UA);
        return h;
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 JSON {"csrf":"...","cookies":"..."}
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(BASE_URL + "/", pageHeaders());
        resp.ensureSuccess();
        String csrf = ProviderUtil.extractCsrfToken(resp.getBody());
        if (csrf.isEmpty()) {
            throw new RuntimeException("tempemails-net: 无法提取 CSRF token");
        }
        String cookie = ProviderUtil.extractAllCookies(resp);
        if (cookie.isEmpty()) {
            throw new RuntimeException("tempemails-net: 未获取到 session cookie");
        }
        // POST /get_messages 获取分配的邮箱
        HttpResult resp2 = HttpClient.post(BASE_URL + "/get_messages", null, null,
                ajaxHeaders(csrf, cookie));
        resp2.ensureSuccess();
        String newCk = ProviderUtil.extractAllCookies(resp2);
        if (!newCk.isEmpty()) {
            cookie = ProviderUtil.mergeCookieStrings(cookie, newCk);
        }
        JsonElement data = Json.parse(resp2.getBody());
        String mailbox = Json.str(data, "mailbox").trim();
        if (mailbox.isEmpty() || !mailbox.contains("@")) {
            throw new RuntimeException("tempemails-net: 邮箱地址无效");
        }
        String token = "{\"csrf\":\"" + csrf + "\",\"cookies\":\"" + cookie.replace("\"", "\\\"") + "\"}";
        return new EmailInfo("tempemails-net", mailbox, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 格式的 CSRF + cookie
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("tempemails-net: 邮箱地址为空");
        }
        JsonElement tokenData = Json.parse(token != null ? token : "{}");
        String csrf = Json.str(tokenData, "csrf").trim();
        String cookie = Json.str(tokenData, "cookies").trim();
        if (csrf.isEmpty() || cookie.isEmpty()) {
            throw new RuntimeException("tempemails-net: token 无效");
        }
        HttpResult resp = HttpClient.post(BASE_URL + "/get_messages", null, null,
                ajaxHeaders(csrf, cookie));
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonArray messages = Json.arr(data, "messages");
        if (messages == null || messages.size() == 0) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : messages) {
            if (item == null || !item.isJsonObject()) continue;
            String msgId = Json.str(item, "id").trim();
            // 获取 HTML 正文
            String htmlBody = fetchView(cookie, msgId);
            // 构造发件人
            String fromName = Json.str(item, "from").trim();
            String fromEmail = Json.str(item, "from_email").trim();
            String fromDisplay = fromName.isEmpty() || fromName.equals(fromEmail)
                    ? fromEmail : fromName + " <" + fromEmail + ">";
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", msgId);
            raw.put("from", fromDisplay);
            raw.put("from_address", fromEmail);
            raw.put("to", address);
            raw.put("subject", Json.str(item, "subject"));
            raw.put("html", htmlBody);
            raw.put("date", Json.str(item, "receivedAt"));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static String fetchView(String cookie, String msgId) {
        if (msgId == null || msgId.isEmpty()) return "";
        try {
            Map<String, String> h = new LinkedHashMap<>();
            h.put("Cookie", cookie);
            h.put("Referer", BASE_URL + "/");
            h.put("User-Agent", UA);
            h.put("Accept", "text/html,*/*");
            HttpResult resp = HttpClient.get(BASE_URL + "/view/" + ProviderUtil.urlEncode(msgId), h);
            return resp.isOk() ? resp.getBody() : "";
        } catch (Exception e) {
            return "";
        }
    }
}
