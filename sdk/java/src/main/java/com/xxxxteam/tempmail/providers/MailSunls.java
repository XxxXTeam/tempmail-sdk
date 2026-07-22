package com.xxxxteam.tempmail.providers;

import com.google.gson.JsonArray;
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
     * 获取邮件列表。
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
            result.add(Normalizer.normalizeEmail(Json.toDict(item), addr));
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
