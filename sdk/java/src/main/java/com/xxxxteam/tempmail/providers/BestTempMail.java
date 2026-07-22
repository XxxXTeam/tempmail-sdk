package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
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
import java.util.UUID;

/**
 * BestTempMail 渠道 — https://best-temp-mail.com
 *
 * <p>POST /api/v3/createEmail 创建邮箱（客户端生成 UUID 作为 intToken）。
 * POST /api/v3/getEmailList 获取邮件。</p>
 */
public final class BestTempMail {

    private static final String BASE = "https://best-temp-mail.com";

    private BestTempMail() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Content-Type", "application/json");
        h.put("Referer", BASE + "/");
        h.put("Origin", BASE);
        return h;
    }

    /**
     * 创建 best-temp-mail 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        String intToken = UUID.randomUUID().toString();
        String payload = Json.serialize(Map.of("intToken", intToken));

        HttpResult resp = HttpClient.post(BASE + "/api/v3/createEmail", payload, "application/json", headers());
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null || !"success".equals(Json.str(body, "status"))) {
            throw new RuntimeException("best-temp-mail: 创建邮箱失败");
        }
        JsonObject data = body.has("data") && body.get("data").isJsonObject()
                ? body.getAsJsonObject("data") : null;
        if (data == null) throw new RuntimeException("best-temp-mail: 响应缺少 data");

        String address = Json.str(data, "address");
        String emailId = Json.str(data, "id");
        String updateTag = Json.str(data, "update_tag");

        if (address.isEmpty() || !address.contains("@")) {
            throw new RuntimeException("best-temp-mail: 返回的邮箱地址无效");
        }

        String token = Json.serialize(Map.of("intToken", intToken, "id", emailId, "update_tag", updateTag));
        return new EmailInfo("best-temp-mail", address, token, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param token JSON 编码的 token
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String token, String email) {
        JsonObject tokData = Json.parseObject(token);
        if (tokData == null) throw new RuntimeException("best-temp-mail: 无效的 token");

        String intToken = Json.str(tokData, "intToken");
        String emailId = Json.str(tokData, "id");
        String updateTag = Json.str(tokData, "update_tag");

        Map<String, Object> reqBody = new LinkedHashMap<>();
        reqBody.put("address", email);
        reqBody.put("id", emailId);
        reqBody.put("intToken", intToken);
        reqBody.put("update_tag", updateTag);

        HttpResult resp = HttpClient.post(BASE + "/api/v3/getEmailList",
                Json.serialize(reqBody), "application/json", headers());
        resp.ensureSuccess();

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null) return new ArrayList<>();

        JsonObject data = body.has("data") && body.get("data").isJsonObject()
                ? body.getAsJsonObject("data") : null;
        if (data == null) return new ArrayList<>();

        if (data.has("hasNewEmail") && !data.get("hasNewEmail").getAsBoolean()) {
            return new ArrayList<>();
        }

        var emails = Json.arr(data, "emails");
        if (emails == null) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : emails) {
            if (item.isJsonObject()) {
                out.add(Normalizer.normalizeEmail(Json.toDict(item), email));
            }
        }
        return out;
    }
}
