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
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * TempInbox 渠道实现 — https://www.tempinbox.xyz
 */
public final class Tempinbox {

    private static final String BASE = "https://endpoint.tempinbox.xyz";
    private static final List<String> DOMAINS = Arrays.asList(
            "tempinbox.xyz", "thepiratebay.cloud", "cryptoblad.nl");

    private static final Map<String, String> HEADERS = new LinkedHashMap<>();

    static {
        HEADERS.put("Accept", "application/json, text/plain, */*");
        HEADERS.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
        HEADERS.put("Cache-Control", "no-cache");
        HEADERS.put("DNT", "1");
        HEADERS.put("Pragma", "no-cache");
        HEADERS.put("Referer", "https://www.tempinbox.xyz/");
        HEADERS.put("sec-ch-ua", "\"Chromium\";v=\"146\", \"Not-A.Brand\";v=\"24\", \"Microsoft Edge\";v=\"146\"");
        HEADERS.put("sec-ch-ua-mobile", "?0");
        HEADERS.put("sec-ch-ua-platform", "\"Windows\"");
        HEADERS.put("sec-fetch-dest", "empty");
        HEADERS.put("sec-fetch-mode", "cors");
        HEADERS.put("sec-fetch-site", "cross-site");
        HEADERS.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
    }

    private Tempinbox() {
    }

    /**
     * 创建临时邮箱（支持指定域名）。
     *
     * @param domain 指定域名，null 或空串则使用随机邮箱端点
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        String addr;
        if (domain != null && !domain.trim().isEmpty()) {
            String d = domain.trim().toLowerCase();
            if (!DOMAINS.contains(d)) {
                throw new RuntimeException("tempinbox: 域名不可用: " + d);
            }
            String user = ProviderUtil.randomString(10);
            addr = user + "@" + d;
            HttpResult resp = HttpClient.get(BASE + "/email/" + addr, HEADERS);
            resp.ensureSuccess();
        } else {
            HttpResult resp = HttpClient.get(BASE + "/email/Random", HEADERS);
            resp.ensureSuccess();
            addr = resp.getBody().trim().replace("\"", "");
        }
        if (addr.isEmpty() || !addr.contains("@")) {
            throw new RuntimeException("tempinbox: 无效的邮箱地址");
        }
        return new EmailInfo("tempinbox", addr);
    }

    /**
     * 获取邮件列表。
     *
     * @param email 邮箱地址
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email) {
        if (email == null || email.trim().isEmpty()) {
            throw new RuntimeException("tempinbox: 邮箱地址为空");
        }
        String e = email.trim();
        HttpResult resp = HttpClient.get(BASE + "/messages/" + e, HEADERS);
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
            result.add(Normalizer.normalizeEmail(Json.toDict(item), e));
        }
        return result;
    }
}
