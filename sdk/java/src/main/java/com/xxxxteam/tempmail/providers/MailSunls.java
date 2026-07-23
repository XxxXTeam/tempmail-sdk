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
 * Mail Sunls 渠道实现 — https://mail.sunls.de
 *
 * <p>无需 token，无需 session。创建邮箱时从 /api/domain 获取域名列表，本地随机生成地址。</p>
 */
public final class MailSunls {

    private static final String BASE = "https://mail.sunls.de";

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json, text/plain, */*");
        HEADERS.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        HEADERS.put("Cache-Control", "no-cache");
        HEADERS.put("DNT", "1");
        HEADERS.put("Pragma", "no-cache");
        HEADERS.put("Referer", "https://mail.sunls.de/");
        HEADERS.put("sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"");
        HEADERS.put("sec-ch-ua-mobile", "?0");
        HEADERS.put("sec-ch-ua-platform", "\"Windows\"");
        HEADERS.put("sec-fetch-dest", "empty");
        HEADERS.put("sec-fetch-mode", "cors");
        HEADERS.put("sec-fetch-site", "same-origin");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    }

    private MailSunls() {
    }

    /**
     * 创建临时邮箱。
     *
     * @return 邮箱信息
     */
    public static EmailInfo generate() {
        List<String> domains = fetchDomains();
        String domain = domains.get(ThreadLocalRandom.current().nextInt(domains.size()));
        String local = ProviderUtil.randomString(10);
        String email = local + "@" + domain;
        return new EmailInfo("mail-sunls", email);
    }

    /**
     * 通过详情接口获取单封邮件完整正文。
     * GET /api/fetch/{id}
     *
     * @param id 邮件 ID
     * @return 详情 JsonObject（含 text/html），失败返回 null
     */
    private static JsonObject fetchDetail(String id) {
        if (id == null || id.isBlank()) return null;
        try {
            HttpResult resp = HttpClient.get(
                    BASE + "/api/fetch/" + ProviderUtil.urlEncode(id), HEADERS);
            if (resp.getStatusCode() < 200 || resp.getStatusCode() >= 300) return null;
            return Json.parseObject(resp.getBody());
        } catch (RuntimeException e) {
            return null;
        }
    }

    /**
     * 从列表条目提取邮件 ID。
     */
    private static String extractId(JsonObject row) {
        for (String key : new String[]{"id", "_id", "mail_id", "messageId", "message_id"}) {
            if (row.has(key) && !row.get(key).isJsonNull()) {
                String s = Json.str(row, key);
                if (!s.isEmpty()) return s;
            }
        }
        return "";
    }

    /**
     * 获取邮件列表。
     * 1. GET /api/fetch?to={email} 拉取列表元数据
     * 2. 对每封邮件 GET /api/fetch/{id} 拉取详情（含完整 text/html）
     * 3. 详情失败时保留列表字段作为回退
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.trim().isEmpty()) {
            throw new RuntimeException("mail-sunls: 邮箱地址为空");
        }
        String addr = email.trim();
        HttpResult resp = HttpClient.get(
                BASE + "/api/fetch?to=" + ProviderUtil.urlEncode(addr), HEADERS);
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
            JsonObject row = item.getAsJsonObject();
            String id = extractId(row);
            /* 无条件调用详情接口，用详情字段覆盖列表字段（确保正文完整） */
            if (!id.isEmpty()) {
                JsonObject detail = fetchDetail(id);
                if (detail != null) {
                    for (Map.Entry<String, JsonElement> entry : detail.entrySet()) {
                        if (entry.getValue() != null && !entry.getValue().isJsonNull()) {
                            row.add(entry.getKey(), entry.getValue());
                        }
                    }
                }
            }
            result.add(Normalizer.normalizeEmail(Json.toDict(row), addr));
        }
        return result;
    }

    private static List<String> fetchDomains() {
        HttpResult resp = HttpClient.get(BASE + "/api/domain", HEADERS);
        resp.ensureSuccess();
        JsonElement data = Json.parse(resp.getBody());
        if (data == null || !data.isJsonArray()) {
            throw new RuntimeException("mail-sunls: 域名列表响应格式无效");
        }
        List<String> out = new ArrayList<>();
        for (JsonElement el : data.getAsJsonArray()) {
            if (el.isJsonPrimitive()) {
                String d = el.getAsString().trim();
                if (!d.isEmpty()) {
                    out.add(d);
                }
            }
        }
        if (out.isEmpty()) {
            throw new RuntimeException("mail-sunls: 无可用域名");
        }
        return out;
    }
}
