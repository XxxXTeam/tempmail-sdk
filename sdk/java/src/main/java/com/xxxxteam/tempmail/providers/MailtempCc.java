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

/**
 * MailTemp.cc 渠道 — https://mailtemp.cc
 *
 * <p>POST /api.php action=inbox 创建邮箱，action=fetch 获取邮件列表，
 * action=view 获取邮件详情。域名: @neocea.com</p>
 */
public final class MailtempCc {

    private static final String BASE = "https://mailtemp.cc";
    private static final String DOMAIN = "neocea.com";

    private MailtempCc() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("Content-Type", "application/x-www-form-urlencoded");
        h.put("Accept", "*/*");
        h.put("Referer", BASE + "/");
        h.put("Origin", BASE);
        return h;
    }

    /**
     * 创建 mailtemp.cc 临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        HttpResult resp = HttpClient.post(BASE + "/api.php", "action=inbox",
                "application/x-www-form-urlencoded", headers());
        resp.ensureSuccess();

        String rawText = resp.getBody().trim();
        // 返回值为 JSON 字符串格式（带引号），如 "vindictiverate"
        String username;
        if (rawText.startsWith("\"") && rawText.endsWith("\"")) {
            username = rawText.substring(1, rawText.length() - 1);
        } else {
            username = rawText;
        }
        if (username.isEmpty()) throw new RuntimeException("mailtemp-cc: 返回的用户名无效");

        String address = username + "@" + DOMAIN;
        return new EmailInfo("mailtemp-cc", address, username, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 用户名（@ 前部分）
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (token == null || token.isBlank()) throw new RuntimeException("mailtemp-cc: token 为空");
        String addr = (email != null && !email.isBlank()) ? email.trim() : token + "@" + DOMAIN;

        HttpResult resp = HttpClient.post(BASE + "/api.php",
                "action=fetch&inbox=" + ProviderUtil.urlEncode(token) + "&last_id=0",
                "application/x-www-form-urlencoded", headers());
        resp.ensureSuccess();

        JsonElement parsed = Json.parse(resp.getBody());
        if (parsed == null || !parsed.isJsonArray()) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : parsed.getAsJsonArray()) {
            if (!item.isJsonObject()) continue;
            JsonObject msg = item.getAsJsonObject();
            String mailId = Json.str(msg, "id");
            if (mailId.isEmpty()) continue;

            // 获取邮件详情
            HttpResult detResp = HttpClient.post(BASE + "/api.php",
                    "action=view&id=" + ProviderUtil.urlEncode(mailId)
                            + "&inbox=" + ProviderUtil.urlEncode(token),
                    "application/x-www-form-urlencoded", headers());
            JsonObject detail = detResp.isOk() ? Json.parseObject(detResp.getBody()) : null;

            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", mailId);
            row.put("from", ProviderUtil.firstNonEmpty(
                    Json.str(msg, "sender_email"), Json.str(msg, "sender")));
            row.put("to", addr);
            row.put("subject", ProviderUtil.firstNonEmpty(
                    Json.str(msg, "subject"), detail != null ? Json.str(detail, "subject") : ""));
            row.put("html", detail != null ? Json.str(detail, "body_html") : "");
            row.put("date", ProviderUtil.firstNonEmpty(
                    Json.str(msg, "received_at"), detail != null ? Json.str(detail, "received_at") : ""));
            out.add(Normalizer.normalizeEmail(row, addr));
        }
        return out;
    }
}
