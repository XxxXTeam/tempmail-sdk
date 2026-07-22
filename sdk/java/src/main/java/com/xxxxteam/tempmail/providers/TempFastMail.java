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
import java.util.List;
import java.util.Map;

/**
 * TempFastMail 渠道 — https://tempfastmail.com
 * POST /api/email-box 创建，GET /api/email-box/{uuid}/emails 收信。
 */
public final class TempFastMail {

    private static final String BASE_URL = "https://tempfastmail.com";
    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0");
    }

    private TempFastMail() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息，token 为 uuid
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.post(BASE_URL + "/api/email-box", null, null, HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String email = Json.str(data, "email").trim();
        String uuid = Json.str(data, "uuid").trim();
        if (email.isEmpty() || !email.contains("@") || uuid.isEmpty()) {
            throw new RuntimeException("tempfastmail: 创建邮箱响应无效");
        }
        return new EmailInfo("tempfastmail", email, uuid, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token uuid
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        String uuid = (token != null ? token : "").trim();
        String address = (email != null ? email : "").trim();
        if (uuid.isEmpty()) {
            throw new RuntimeException("tempfastmail: token 为空");
        }
        if (address.isEmpty()) {
            throw new RuntimeException("tempfastmail: 邮箱地址为空");
        }
        HttpResult resp = HttpClient.get(
                BASE_URL + "/api/email-box/" + ProviderUtil.urlEncode(uuid) + "/emails", HEADERS);
        resp.ensureSuccess();
        JsonElement parsed = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (parsed == null || !parsed.isJsonArray()) {
            return result;
        }
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (item == null || !item.isJsonObject()) {
                continue;
            }
            String msgUuid = Json.str(item, "uuid").trim();
            JsonElement detail = null;
            if (!msgUuid.isEmpty()) {
                detail = fetchDetail(uuid, msgUuid);
            }
            Map<String, Object> raw = buildRaw(detail != null ? detail : item, address);
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }

    private static JsonElement fetchDetail(String boxUuid, String msgUuid) {
        try {
            HttpResult resp = HttpClient.get(
                    BASE_URL + "/api/email-box/" + ProviderUtil.urlEncode(boxUuid)
                            + "/email/" + ProviderUtil.urlEncode(msgUuid), HEADERS);
            if (!resp.isOk()) {
                return null;
            }
            return Json.parse(resp.getBody());
        } catch (Exception e) {
            return null;
        }
    }

    private static Map<String, Object> buildRaw(JsonElement el, String recipient) {
        Map<String, Object> raw = Json.toDict(el);
        raw.put("id", raw.getOrDefault("uuid", ""));
        raw.putIfAbsent("to", raw.getOrDefault("real_to", recipient));
        raw.putIfAbsent("date", raw.getOrDefault("received_at", ""));
        return raw;
    }
}
