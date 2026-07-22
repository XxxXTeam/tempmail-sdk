package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
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
 * GuerrillaMail 及其全部镜像渠道实现。
 *
 * <p>所有镜像共用同一套 ajax.php API，仅 baseUrl 不同：
 * guerrillamail.com / sharklasers.com / grr.la / guerrillamail.info / spam4.me 等。</p>
 */
public final class GuerrillaMail {

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    }

    private GuerrillaMail() {
    }

    /**
     * 创建临时邮箱（指定镜像 baseUrl 与渠道标识）。
     *
     * @param baseUrl API 基地址
     * @param channel 对外渠道标识
     * @return 邮箱信息
     */
    public static EmailInfo generate(String baseUrl, String channel) {
        HttpResult resp = HttpClient.get(baseUrl + "?f=get_email_address&lang=en", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        String email = Json.str(data, "email_addr");
        String token = Json.str(data, "sid_token");
        if (email.isEmpty() || token.isEmpty()) {
            throw new RuntimeException(channel + ": missing email_addr or sid_token");
        }

        Long expiresAt = null;
        if (data != null && data.isJsonObject()) {
            JsonObject o = data.getAsJsonObject();
            if (o.has("email_timestamp") && !o.get("email_timestamp").isJsonNull()) {
                try {
                    long t = o.get("email_timestamp").getAsLong();
                    expiresAt = (t + 3600) * 1000;
                } catch (RuntimeException ignored) {
                    // 时间戳解析失败则不设置过期时间
                }
            }
        }
        return new EmailInfo(channel, email, token, expiresAt, null);
    }

    /**
     * 获取邮件列表，对每封调用 fetch_email 拉正文。
     *
     * @param baseUrl API 基地址
     * @param token   会话令牌
     * @param email   邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String baseUrl, String token, String email) {
        String tok = token != null ? token : "";
        HttpResult resp = HttpClient.get(
                baseUrl + "?f=check_email&seq=0&sid_token=" + ProviderUtil.urlEncode(tok), HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());

        JsonArray list = Json.arr(data, "list");
        List<Email> result = new ArrayList<>();
        if (list == null) {
            return result;
        }

        for (JsonElement item : list) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject msg = item.getAsJsonObject();
            String body = Json.str(msg, "mail_body");
            String mailId = Json.str(msg, "mail_id");
            if (body.isEmpty() && !mailId.isEmpty()) {
                try {
                    HttpResult dr = HttpClient.get(baseUrl + "?f=fetch_email&sid_token="
                            + ProviderUtil.urlEncode(tok) + "&email_id=" + ProviderUtil.urlEncode(mailId), HEADERS);
                    if (dr.isOk()) {
                        body = Json.str(Json.parse(dr.getBody()), "mail_body");
                    }
                } catch (RuntimeException ignored) {
                    // 忽略单封失败
                }
            }
            String text = body.isEmpty() ? Json.str(msg, "mail_excerpt") : Normalizer.htmlToText(body);
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", mailId);
            raw.put("from", Json.str(msg, "mail_from"));
            raw.put("to", email);
            raw.put("subject", Json.str(msg, "mail_subject"));
            raw.put("text", text);
            raw.put("html", body);
            raw.put("date", Json.str(msg, "mail_date"));
            raw.put("isRead", "1".equals(Json.str(msg, "mail_read")));
            result.add(Normalizer.normalizeEmail(raw, email));
        }
        return result;
    }
}
