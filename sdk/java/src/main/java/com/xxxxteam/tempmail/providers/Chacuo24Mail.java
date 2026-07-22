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
 * 24mail.chacuo.net 渠道 — http://24mail.chacuo.net
 *
 * <p>POST / form: data={email}&type=refresh&arg= 创建和刷新邮箱。</p>
 */
public final class Chacuo24Mail {

    private static final String BASE_URL = "http://24mail.chacuo.net/";
    private static final String DEFAULT_DOMAIN = "chacuo.net";

    private Chacuo24Mail() {
    }

    private static Map<String, String> headers() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0");
        h.put("Accept", "application/json, text/javascript, */*; q=0.01");
        h.put("X-Requested-With", "XMLHttpRequest");
        h.put("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
        h.put("Origin", "http://24mail.chacuo.net");
        h.put("Referer", "http://24mail.chacuo.net/");
        return h;
    }

    private static JsonObject refresh(String email) {
        String body = "data=" + ProviderUtil.urlEncode(email) + "&type=refresh&arg=";
        HttpResult resp = HttpClient.post(BASE_URL, body,
                "application/x-www-form-urlencoded; charset=UTF-8", headers());
        resp.ensureSuccess();
        JsonObject obj = Json.parseObject(resp.getBody());
        if (obj == null) throw new RuntimeException("24mail-chacuo: 响应格式异常");
        return obj;
    }

    /**
     * 创建 24mail.chacuo.net 临时邮箱。
     *
     * @param domain 首选域名，可为 null
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        String dom = (domain != null && !domain.isBlank()) ? domain.trim() : DEFAULT_DOMAIN;
        String email = "sdk" + ProviderUtil.randomString(12) + "@" + dom;
        JsonObject result = refresh(email);
        if (!result.has("status") || result.get("status").getAsInt() != 1) {
            throw new RuntimeException("24mail-chacuo: 创建邮箱失败");
        }
        return new EmailInfo("24mail-chacuo", email, email, null, null);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 邮箱地址（与 email 相同）
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String addr = (email != null && !email.isBlank()) ? email.trim()
                : (token != null ? token.trim() : "");
        if (addr.isEmpty()) throw new RuntimeException("24mail-chacuo: 邮箱地址为空");

        JsonObject result = refresh(addr);
        JsonArray dataArr = Json.arr(result, "data");
        if (dataArr == null || dataArr.isEmpty()) return new ArrayList<>();

        JsonElement first = dataArr.get(0);
        if (!first.isJsonObject()) return new ArrayList<>();
        JsonArray rows = Json.arr(first, "list");
        if (rows == null || rows.isEmpty()) return new ArrayList<>();

        List<Email> out = new ArrayList<>();
        for (JsonElement item : rows) {
            if (!item.isJsonObject()) continue;
            JsonObject msg = item.getAsJsonObject();
            Map<String, Object> row = new LinkedHashMap<>();
            row.put("id", Json.str(msg, "MID"));
            row.put("from", Json.str(msg, "FROM"));
            row.put("to", addr);
            row.put("subject", Json.str(msg, "SUBJECT"));
            row.put("content", Json.str(msg, "CONTENT"));
            row.put("date", Json.str(msg, "TIME"));
            out.add(Normalizer.normalizeEmail(row, addr));
        }
        return out;
    }
}
