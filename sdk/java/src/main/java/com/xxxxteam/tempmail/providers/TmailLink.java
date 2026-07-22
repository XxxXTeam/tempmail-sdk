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
 * tmail.link 渠道 — https://tmail.link
 * GET / 正则提取邮箱，GET /inbox/{email}/ 获取 csrftoken cookie，
 * POST /inbox/{email}/ form: format=json&csrfmiddlewaretoken={token} 收信。
 */
public final class TmailLink {

    private static final String BASE_URL = "https://tmail.link";
    private static final String UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private TmailLink() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 csrftoken 值
     */
    public static EmailInfo generate() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        // GET / 提取邮箱
        HttpResult resp = HttpClient.get(BASE_URL + "/", h);
        resp.ensureSuccess();
        String address = ProviderUtil.regexExtract(resp.getBody(),
                "([a-zA-Z0-9._%+-]+@tmail\\.link)");
        if (address.isEmpty()) {
            throw new RuntimeException("tmail-link: 未能从首页提取邮箱地址");
        }
        // GET /inbox/{email}/ 获取 csrftoken cookie
        HttpResult resp2 = HttpClient.get(BASE_URL + "/inbox/" + address + "/", h);
        resp2.ensureSuccess();
        String csrfToken = "";
        for (String raw : resp2.getSetCookies()) {
            if (raw.contains("csrftoken=")) {
                String seg = raw.substring(raw.indexOf("csrftoken=") + "csrftoken=".length());
                int semi = seg.indexOf(';');
                csrfToken = semi < 0 ? seg : seg.substring(0, semi);
                break;
            }
        }
        if (csrfToken.isEmpty()) {
            throw new RuntimeException("tmail-link: 未能获取 csrftoken");
        }
        return new EmailInfo("tmail-link", address, csrfToken, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token csrftoken 值
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        String csrf = (token != null ? token : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("tmail-link: 邮箱地址为空");
        }
        if (csrf.isEmpty()) {
            throw new RuntimeException("tmail-link: token 为空");
        }
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
        h.put("Origin", BASE_URL);
        h.put("Referer", BASE_URL + "/inbox/" + address + "/");
        h.put("Cookie", "csrftoken=" + csrf);
        String body = "format=json&csrfmiddlewaretoken=" + ProviderUtil.urlEncode(csrf);
        HttpResult resp = HttpClient.post(BASE_URL + "/inbox/" + address + "/", body,
                "application/x-www-form-urlencoded", h);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonArray messages = Json.arr(data, "messages");
        if (messages == null || messages.size() == 0) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : messages) {
            if (item == null || !item.isJsonObject()) continue;
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(item, "key"));
            raw.put("from", Json.str(item, "sender"));
            raw.put("to", address);
            raw.put("subject", Json.str(item, "subject"));
            raw.put("date", Json.str(item, "date"));
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }
}
