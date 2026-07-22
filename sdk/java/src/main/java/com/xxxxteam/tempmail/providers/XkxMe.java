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
 * xkx.me 渠道 — https://xkx.me
 * CSRF + session cookie 认证，POST /mailbox/create/random 创建（302 redirect），
 * GET /mailbox/{email}/messages 收信。
 */
public final class XkxMe {

    private static final String BASE_URL = "https://xkx.me";
    private static final String UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36";

    private XkxMe() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 session cookie 字符串
     */
    public static EmailInfo generate() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        // GET / 获取 CSRF + session cookie
        HttpResult resp = HttpClient.get(BASE_URL, h);
        resp.ensureSuccess();
        String csrf = ProviderUtil.regexExtract(resp.getBody(), "csrf-token\"\\s+content=\"([^\"]+)\"");
        if (csrf.isEmpty()) {
            throw new RuntimeException("xkx-me: 无法获取 CSRF token");
        }
        String cookie = ProviderUtil.extractAllCookies(resp);
        if (cookie.isEmpty()) {
            throw new RuntimeException("xkx-me: 未获取到 session cookie");
        }
        // POST /mailbox/create/random（不跟随重定向）
        Map<String, String> postH = new LinkedHashMap<>();
        postH.put("User-Agent", UA);
        postH.put("Content-Type", "application/x-www-form-urlencoded");
        postH.put("Cookie", cookie);
        postH.put("Referer", BASE_URL + "/");
        String body = "_token=" + ProviderUtil.urlEncode(csrf);
        HttpResult resp2 = HttpClient.postNoRedirect(BASE_URL + "/mailbox/create/random", body,
                "application/x-www-form-urlencoded", postH);
        // 从 Location 头提取邮箱
        String location = resp2.getHeaders().getOrDefault("Location",
                resp2.getHeaders().getOrDefault("location", ""));
        String address = ProviderUtil.regexExtract(location, "mailbox/([^/\\s\"'<>]+@xkx\\.me)");
        if (address.isEmpty()) {
            throw new RuntimeException("xkx-me: 无法从响应中提取邮箱地址");
        }
        // 合并 cookie
        String newCk = ProviderUtil.extractAllCookies(resp2);
        if (!newCk.isEmpty()) {
            cookie = ProviderUtil.mergeCookieStrings(cookie, newCk);
        }
        return new EmailInfo("xkx-me", address, cookie, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token session cookie 字符串
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        String cookie = (token != null ? token : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("xkx-me: 邮箱地址为空");
        }
        if (cookie.isEmpty()) {
            throw new RuntimeException("xkx-me: token 为空");
        }
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "application/json");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Cookie", cookie);
        HttpResult resp = HttpClient.get(
                BASE_URL + "/mailbox/" + ProviderUtil.urlEncode(address) + "/messages", h);
        if (resp.getStatusCode() == 404) {
            return new ArrayList<>();
        }
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        // 响应可能是数组或对象
        List<JsonElement> messageList = new ArrayList<>();
        if (data != null && data.isJsonArray()) {
            for (JsonElement el : data.getAsJsonArray()) messageList.add(el);
        } else if (data != null && data.isJsonObject()) {
            JsonArray msgs = Json.arr(data, "messages");
            if (msgs != null) {
                for (JsonElement el : msgs) messageList.add(el);
            } else {
                JsonElement msg = data.getAsJsonObject().get("message");
                if (msg != null && msg.isJsonObject()) messageList.add(msg);
            }
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : messageList) {
            if (item == null || !item.isJsonObject()) continue;
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(item, "id"));
            raw.put("from", Json.str(item, "from"));
            raw.put("to", address);
            raw.put("subject", Json.str(item, "subject"));
            raw.put("date", Json.str(item, "date"));
            raw.put("html", ProviderUtil.firstNonEmpty(Json.str(item, "html"), Json.str(item, "body")));
            raw.put("text", Json.str(item, "text"));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }
}
