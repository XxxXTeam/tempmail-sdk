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
 * tempemail.co 渠道 — https://tempemail.co
 * GET /mail/random 创建，GET /get-mails 获取邮件列表（HTML regex 提取 ID），
 * GET /mail/info?id={id} 获取邮件详情。token 为 JSON {"address":"...","cookies":"..."}。
 */
public final class TempEmailCo {

    private static final String BASE_URL = "https://tempemail.co";
    private static final Pattern MAIL_ID_RE = Pattern.compile("data-id=\"(\\d+)\"");
    private static final String UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private TempEmailCo() {
    }

    private static Map<String, String> jsonHeaders(String cookie) {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", UA);
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("Referer", BASE_URL + "/");
        if (cookie != null && !cookie.isEmpty()) {
            h.put("Cookie", cookie);
        }
        return h;
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 JSON {"address":"...","cookies":"..."}
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.get(BASE_URL + "/mail/random", jsonHeaders(""));
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonElement resultEl = data.getAsJsonObject().get("result");
        if (resultEl == null || !resultEl.getAsBoolean()) {
            throw new RuntimeException("tempemail-co: 创建邮箱失败");
        }
        String address = ProviderUtil.firstNonEmpty(
                Json.str(data, "address"), Json.str(data, "id")).trim();
        if (address.isEmpty() || !address.contains("@")) {
            throw new RuntimeException("tempemail-co: 邮箱地址无效");
        }
        String cookie = ProviderUtil.extractAllCookies(resp);
        String token = "{\"address\":\"" + address + "\",\"cookies\":\"" + cookie.replace("\"", "\\\"") + "\"}";
        return new EmailInfo("tempemail-co", address, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 格式认证信息
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("tempemail-co: 邮箱地址为空");
        }
        JsonElement tokenData = Json.parse(token != null ? token : "{}");
        String storedAddr = Json.str(tokenData, "address").trim();
        String cookie = Json.str(tokenData, "cookies").trim();
        String queryAddr = storedAddr.isEmpty() ? address : storedAddr;
        // GET /get-mails
        String url = BASE_URL + "/get-mails?mail_id=" + ProviderUtil.urlEncode(queryAddr)
                + "&unseen=0&is_new=1";
        HttpResult resp = HttpClient.get(url, jsonHeaders(cookie));
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        JsonElement countEl = data.getAsJsonObject().get("count");
        int count = 0;
        if (countEl != null && countEl.isJsonPrimitive()) {
            try { count = countEl.getAsInt(); } catch (Exception ignored) {}
        }
        if (count <= 0) {
            return new ArrayList<>();
        }
        // 从 mails HTML 中提取邮件 ID
        String mailsHtml = Json.str(data, "mails");
        if (mailsHtml.isEmpty()) {
            return new ArrayList<>();
        }
        Set<String> uniqueIds = new LinkedHashSet<>();
        Matcher m = MAIL_ID_RE.matcher(mailsHtml);
        while (m.find()) {
            uniqueIds.add(m.group(1));
        }
        if (uniqueIds.isEmpty()) {
            return new ArrayList<>();
        }
        List<Email> result = new ArrayList<>();
        for (String mailId : uniqueIds) {
            Map<String, Object> raw = fetchMailInfo(cookie, mailId, address);
            if (raw != null) {
                result.add(Normalizer.normalizeEmail(raw, address));
            }
        }
        return result;
    }

    private static Map<String, Object> fetchMailInfo(String cookie, String mailId, String recipient) {
        try {
            String url = BASE_URL + "/mail/info?id=" + ProviderUtil.urlEncode(mailId);
            HttpResult resp = HttpClient.get(url, jsonHeaders(cookie));
            if (!resp.isOk()) return null;
            JsonElement data = Json.parse(resp.getBody());
            JsonElement resultEl = data.getAsJsonObject().get("result");
            if (resultEl == null || !resultEl.getAsBoolean()) return null;
            JsonElement mailEl = data.getAsJsonObject().get("mail");
            if (mailEl == null || !mailEl.isJsonObject()) return null;
            String fromName = Json.str(mailEl, "fromName").trim();
            String fromAddr = Json.str(mailEl, "fromAddress").trim();
            String fromDisplay = fromName.isEmpty() ? fromAddr : fromName + " <" + fromAddr + ">";
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", mailId);
            raw.put("from", fromDisplay);
            raw.put("from_address", fromAddr);
            raw.put("to", recipient);
            raw.put("subject", Json.str(mailEl, "subject"));
            raw.put("html", Json.str(mailEl, "textHtml"));
            raw.put("date", Json.str(mailEl, "displayDate"));
            return raw;
        } catch (Exception e) {
            return null;
        }
    }
}
