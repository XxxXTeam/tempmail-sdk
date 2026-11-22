package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
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
 * PlainlyKit 渠道（plainlykit.com）。
 *
 * <p>创建邮箱返回内部令牌；内部令牌格式为 {@code 邮箱ID|读取令牌}。</p>
 */
public final class PlainlyKit {

    private static final String API_BASE =
            "https://plainlykit.com/tools/temporary-email/api";

    private PlainlyKit() {
    }

    private static Map<String, String> bearerHeaders(String bearer) {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("Accept", "application/json");
        headers.put("Authorization", "Bearer " + bearer);
        return headers;
    }

    /**
     * 创建 PlainlyKit 临时邮箱。
     *
     * @param channel 渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String channel) {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("Accept", "application/json");
        headers.put("Content-Type", "application/json");
        HttpResult resp = HttpClient.post(API_BASE + "/mailboxes", "{}", "application/json", headers);
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        String address = data == null ? "" : Json.str(data, "address").trim();
        String id = data == null ? "" : Json.str(data, "id").trim();
        String token = data == null ? "" : Json.str(data, "token").trim();
        if (address.isEmpty() || id.isEmpty() || token.isEmpty()) {
            throw new RuntimeException("plainlykit: incomplete creation response");
        }
        return new EmailInfo(channel, address, id + "|" + token, null, Json.str(data, "createdAt"));
    }

    /**
     * 获取 PlainlyKit 邮件，详情提供纯文本正文。
     *
     * @param email 邮箱地址
     * @param token 内部令牌
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (token == null || !token.contains("|")) {
            throw new IllegalArgumentException("plainlykit: malformed internal token");
        }
        String[] parts = token.split("\\|", 2);
        String base = API_BASE + "/mailboxes/" + ProviderUtil.urlEncode(parts[0]) + "/messages";
        HttpResult resp = HttpClient.get(base, bearerHeaders(parts[1]));
        resp.ensureSuccess();
        JsonObject data = Json.parseObject(resp.getBody());
        JsonArray rows = Json.arr(data, "messages");
        List<Email> result = new ArrayList<>();
        if (rows == null) {
            return result;
        }
        for (JsonElement row : rows) {
            if (!row.isJsonObject()) {
                continue;
            }
            JsonObject item = row.getAsJsonObject();
            String messageId = Json.str(item, "id").trim();
            if (messageId.isEmpty()) {
                continue;
            }
            JsonObject detail = fetchDetail(base, messageId, parts[1]);
            if (detail != null) {
                JsonObject raw = detail.has("message") && detail.get("message").isJsonObject()
                        ? detail.getAsJsonObject("message") : detail;
                Map<String, Object> flat = new LinkedHashMap<>();
                flat.put("id", ProviderUtil.firstNonEmpty(Json.str(raw, "id"), messageId));
                flat.put("from", ProviderUtil.firstNonEmpty(
                        Json.str(raw, "sender"), Json.str(item, "sender")));
                flat.put("to", email);
                flat.put("subject", ProviderUtil.firstNonEmpty(
                        Json.str(raw, "subject"), Json.str(item, "subject")));
                flat.put("text", Json.str(raw, "text"));
                flat.put("html", Json.str(raw, "html"));
                flat.put("date", Json.str(raw, "receivedAt"));
                result.add(Normalizer.normalizeEmail(flat, email));
                continue;
            }
            Map<String, Object> flat = new LinkedHashMap<>();
            flat.put("id", messageId);
            flat.put("from", Json.str(item, "sender"));
            flat.put("to", email);
            flat.put("subject", Json.str(item, "subject"));
            flat.put("text", Json.str(item, "preview"));
            flat.put("date", Json.str(item, "receivedAt"));
            result.add(Normalizer.normalizeEmail(flat, email));
        }
        return result;
    }

    private static JsonObject fetchDetail(String base, String messageId, String bearer) {
        try {
            HttpResult resp = HttpClient.get(
                    base + "/" + ProviderUtil.urlEncode(messageId), bearerHeaders(bearer));
            if (!resp.isOk()) {
                return null;
            }
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException ignored) {
            return null;
        }
    }
}
