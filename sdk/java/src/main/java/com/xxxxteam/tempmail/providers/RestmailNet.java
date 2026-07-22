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
import java.util.concurrent.ThreadLocalRandom;

/**
 * restmail-net 渠道实现 — https://restmail.net
 *
 * <p>Mozilla 开源项目，无需创建邮箱，ad-hoc 模式。
 * 随机生成 username，邮箱即为 username@restmail.net。</p>
 */
public final class RestmailNet {

    private static final String BASE_URL = "https://restmail.net";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36");
    }

    private RestmailNet() {
    }

    /**
     * 创建 restmail.net 临时邮箱（无需请求服务端）。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        int length = 10 + ThreadLocalRandom.current().nextInt(3); // 10-12
        String username = ProviderUtil.randomString(length);
        String email = username + "@restmail.net";
        return new EmailInfo("restmail-net", email);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        String address = (email != null ? email : "").trim();
        if (address.isEmpty()) {
            throw new RuntimeException("restmail-net: 邮箱地址为空");
        }
        String username = address.split("@")[0];
        if (username.isEmpty()) {
            throw new RuntimeException("restmail-net: 无法提取用户名");
        }
        HttpResult resp = HttpClient.get(BASE_URL + "/mail/" + username, HEADERS);
        if (resp.getStatusCode() == 404) {
            return new ArrayList<>();
        }
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        List<Email> result = new ArrayList<>();
        if (data == null || !data.isJsonArray()) {
            return result;
        }
        for (JsonElement item : data.getAsJsonArray()) {
            if (!item.isJsonObject()) {
                continue;
            }
            JsonObject msg = item.getAsJsonObject();
            // 提取发件人：优先 from[0].address，其次 headers.from
            String fromAddr = "";
            JsonElement fromEl = msg.get("from");
            if (fromEl != null && fromEl.isJsonArray()) {
                JsonArray fromArr = fromEl.getAsJsonArray();
                if (!fromArr.isEmpty() && fromArr.get(0).isJsonObject()) {
                    fromAddr = Json.str(fromArr.get(0), "address");
                }
            }
            if (fromAddr.isEmpty()) {
                JsonElement headersEl = msg.get("headers");
                if (headersEl != null && headersEl.isJsonObject()) {
                    fromAddr = Json.str(headersEl, "from");
                }
            }
            Map<String, Object> raw = new LinkedHashMap<>();
            raw.put("id", Json.str(msg, "id"));
            raw.put("from", fromAddr);
            raw.put("to", address);
            raw.put("subject", Json.str(msg, "subject"));
            raw.put("text", Json.str(msg, "text"));
            raw.put("html", Json.str(msg, "html"));
            raw.put("date", Json.str(msg, "receivedAt"));
            raw.put("is_read", false);
            result.add(Normalizer.normalizeEmail(raw, address));
        }
        return result;
    }
}
