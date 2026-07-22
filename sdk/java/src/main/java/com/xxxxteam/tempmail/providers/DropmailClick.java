package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * DropMail.click 渠道（dropmail.click）。
 * 独立后端临时邮箱，免注册、无鉴权。Token 即邮箱地址本身。
 */
public final class DropmailClick {

    private static final String BASE_URL = "https://dropmail.click";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
        HEADERS.put("Accept", "application/json");
    }

    private DropmailClick() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.post(BASE_URL + "/api/v1/public/mailbox", null, "application/json", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonObject()) {
            throw new RuntimeException("dropmail-click: 无效响应");
        }
        String address = Json.str(parsed, "address").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("dropmail-click: 无效响应，缺少 address");
        }
        return new EmailInfo("dropmail-click", address, address, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token token（即邮箱地址）
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String addr = (token != null && !token.isBlank()) ? token.trim() : (email != null ? email.trim() : "");
        if (addr.isEmpty()) {
            throw new RuntimeException("dropmail-click: 缺少邮箱地址");
        }
        HttpResult resp = HttpClient.get(
                BASE_URL + "/api/v1/public/mailbox/" + ProviderUtil.urlEncode(addr), HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonObject()) {
            return result;
        }
        JsonElement msgsEl = parsed.getAsJsonObject().get("messages");
        if (msgsEl == null || !msgsEl.isJsonArray()) {
            return result;
        }
        for (JsonElement item : msgsEl.getAsJsonArray()) {
            if (!item.isJsonObject()) {
                continue;
            }
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(item, "id"));
            raw.put("from", Json.str(item, "from"));
            raw.put("to", ProviderUtil.firstNonEmpty(Json.str(item, "address"), addr));
            raw.put("subject", Json.str(item, "subject"));
            raw.put("text", Json.str(item, "text"));
            raw.put("html", Json.str(item, "html"));
            raw.put("date", ProviderUtil.firstNonEmpty(Json.str(item, "received_at"), Json.str(item, "date")));
            result.add(Normalizer.normalizeEmail(raw, addr));
        }
        return result;
    }
}
