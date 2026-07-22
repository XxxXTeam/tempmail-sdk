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
 * Rootsh(BccTo) 渠道 — https://rootsh.com
 * GET / 获取 session cookie，POST /applymail 创建，POST /getmail 收信，POST /viewmail 获取正文。
 * token 使用 JSON 前缀 "rootsh1:"，内含 lastCheckTime 和 cookies。
 */
public final class Rootsh {

    private static final String BASE_URL = "https://rootsh.com";
    private static final String TOK_PREFIX = "rootsh1:";
    private static final String DEFAULT_DOMAIN = "bccto.cc";
    private static final String UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private Rootsh() {
    }

    private static Map<String, String> apiHeaders(String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Accept", "application/json, text/plain, */*");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        h.put("Referer", BASE_URL + "/");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
        h.put("User-Agent", UA);
        h.put("Cookie", cookie);
        return h;
    }

    private static String encodeToken(long lastCheckTime, String cookie) {
        return TOK_PREFIX + "{\"t\":" + lastCheckTime + ",\"c\":\"" + cookie.replace("\"", "\\\"") + "\"}";
    }

    private static Object[] decodeToken(String token) {
        if (token == null || !token.startsWith(TOK_PREFIX)) {
            throw new RuntimeException("rootsh: 无效的 session token");
        }
        JsonElement obj = Json.parse(token.substring(TOK_PREFIX.length()));
        long t = 0;
        JsonElement tEl = obj.getAsJsonObject().get("t");
        if (tEl != null && tEl.isJsonPrimitive()) {
            t = tEl.getAsLong();
        }
        String c = Json.str(obj, "c").trim();
        return new Object[]{t, c};
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String local = ProviderUtil.randomString(10);
        String mailAddr = local + "@" + DEFAULT_DOMAIN;
        // GET / 获取 session cookie
        Map<String, String> pageH = new LinkedHashMap<>();
        pageH.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        pageH.put("User-Agent", UA);
        HttpResult resp = HttpClient.get(BASE_URL + "/", pageH);
        resp.ensureSuccess();
        String cookie = ProviderUtil.extractAllCookies(resp);
        // POST /applymail
        String body = "mail=" + ProviderUtil.urlEncode(mailAddr);
        HttpResult resp2 = HttpClient.post(BASE_URL + "/applymail", body,
                "application/x-www-form-urlencoded", apiHeaders(cookie));
        resp2.ensureSuccess();
        // 合并 cookie
        String newCk = ProviderUtil.extractAllCookies(resp2);
        if (!newCk.isEmpty()) {
            cookie = ProviderUtil.mergeCookieStrings(cookie, newCk);
        }
        JsonElement data = Json.parse(resp2.getBody());
        String success = Json.str(data, "success");
        if (!"true".equals(success)) {
            String tips = Json.str(data, "tips");
            throw new RuntimeException("rootsh: 邮箱申请失败: " + tips);
        }
        String actualEmail = Json.str(data, "user").trim();
        if (actualEmail.isEmpty()) actualEmail = mailAddr;
        long lastCheckTime = 0;
        JsonElement timeEl = data.getAsJsonObject().get("time");
        if (timeEl != null && timeEl.isJsonPrimitive()) {
            try { lastCheckTime = timeEl.getAsLong(); } catch (Exception ignored) {}
        }
        return new EmailInfo("rootsh", actualEmail, encodeToken(lastCheckTime, cookie), null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 格式的认证信息
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("rootsh: 邮箱地址为空");
        }
        Object[] parts = decodeToken(token);
        long lastCheckTime = (Long) parts[0];
        String cookie = (String) parts[1];
        long ts = System.currentTimeMillis();
        // POST /getmail
        String body = "mail=" + ProviderUtil.urlEncode(address)
                + "&time=" + lastCheckTime + "&_=" + ts;
        HttpResult resp = HttpClient.post(BASE_URL + "/getmail", body,
                "application/x-www-form-urlencoded", apiHeaders(cookie));
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String success = Json.str(data, "success");
        if (!"true".equals(success)) {
            return new ArrayList<>();
        }
        JsonArray mailList = Json.arr(data, "mail");
        if (mailList == null || mailList.size() == 0) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (JsonElement item : mailList) {
            if (item == null || !item.isJsonArray()) continue;
            JsonArray arr = item.getAsJsonArray();
            if (arr.size() < 5) continue;
            String fromEmail = arr.get(1).isJsonNull() ? "" : arr.get(1).getAsString();
            String subject = arr.get(2).isJsonNull() ? "" : arr.get(2).getAsString();
            String dateStr = arr.get(3).isJsonNull() ? "" : arr.get(3).getAsString();
            String mailFid = arr.get(4).isJsonNull() ? "" : arr.get(4).getAsString();
            // POST /viewmail 获取正文
            String htmlContent = fetchViewMail(cookie, mailFid, address);
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", mailFid);
            raw.put("from", fromEmail);
            raw.put("to", address);
            raw.put("subject", subject);
            raw.put("date", dateStr);
            raw.put("html", htmlContent);
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static String fetchViewMail(String cookie, String mailFid, String toAddr) {
        if (mailFid == null || mailFid.isEmpty()) return "";
        try {
            String body = "mail=" + ProviderUtil.urlEncode(mailFid)
                    + "&to=" + ProviderUtil.urlEncode(toAddr);
            HttpResult resp = HttpClient.post(BASE_URL + "/viewmail", body,
                    "application/x-www-form-urlencoded", apiHeaders(cookie));
            if (!resp.isOk()) return "";
            JsonElement data = Json.parse(resp.getBody());
            String success = Json.str(data, "success");
            if ("true".equals(success)) {
                return Json.str(data, "mail");
            }
            return "";
        } catch (Exception e) {
            return "";
        }
    }
}
