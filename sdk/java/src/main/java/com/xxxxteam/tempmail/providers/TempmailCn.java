package com.xxxxteam.tempmail.providers;

import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * tempmail-cn 渠道实现（tempmail.cn）。
 *
 * <p>generate: Socket.IO 连接 → request mailbox → 接收 mailbox 事件；
 * getEmails: REST API GET https://{host}/api/mails/{email}。</p>
 */
public final class TempmailCn {

    private static final String CHANNEL = "tempmail-cn";
    private static final String DEFAULT_HOST = "tempmail.cn";

    private TempmailCn() {
    }

    private static Map<String, String> baseHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6");
        h.put("Cache-Control", "no-cache");
        h.put("DNT", "1");
        h.put("Pragma", "no-cache");
        return h;
    }

    /**
     * 通过 Socket.IO 请求邮箱地址。
     *
     * @param domain 可选域名/主机，为空则使用默认主机
     * @return 邮箱信息
     */
    public static EmailInfo generate(String domain) {
        String host = normalizeHost(domain);
        SocketIoClient sio = null;
        try {
            sio = SocketIoClient.connect(host, 15);
            sio.emit("request mailbox", "true");
            String payload = sio.waitForEvent("mailbox", 15000);
            String mailbox = SocketIoClient.jsonStringValue(payload).trim();
            if (mailbox.isEmpty()) {
                throw new RuntimeException(CHANNEL + ": mailbox 为空");
            }
            return new EmailInfo(CHANNEL, mailbox + "@" + host);
        } catch (RuntimeException e) {
            throw e;
        } catch (Exception e) {
            throw new RuntimeException(CHANNEL + ": generate 失败: " + e.getMessage(), e);
        } finally {
            if (sio != null) sio.close();
        }
    }

    /**
     * 通过 REST API 获取邮件列表。
     *
     * @param email 邮箱地址
     * @param token 认证令牌（未使用）
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        String trimmed = email == null ? "" : email.trim();
        int atPos = trimmed.indexOf('@');
        if (atPos <= 0) {
            throw new IllegalArgumentException(CHANNEL + ": invalid email address");
        }
        String host = trimmed.substring(atPos + 1);
        if (host.isEmpty()) host = DEFAULT_HOST;

        String url = "https://" + host + "/api/mails/"
                + URLEncoder.encode(trimmed, StandardCharsets.UTF_8);
        Map<String, String> headers = baseHeaders();
        headers.put("Accept", "application/json");
        headers.put("Referer", "https://" + host + "/");

        HttpResult resp = HttpClient.get(url, headers);
        if (resp.getStatusCode() >= 400) {
            return new ArrayList<>();
        }

        JsonObject data = Json.parseObject(resp.getBody());
        if (data == null) {
            return new ArrayList<>();
        }
        JsonArray mails = Json.arr(data, "mails");
        if (mails == null) {
            return new ArrayList<>();
        }

        List<Email> result = new ArrayList<>();
        for (JsonElement item : mails) {
            if (!item.isJsonObject()) continue;
            Map<String, Object> flat = flattenMail(item.getAsJsonObject(), trimmed);
            Email em = Normalizer.normalizeEmail(flat, trimmed);
            if (em.getId() != null && !em.getId().isEmpty()) {
                result.add(em);
            }
        }
        return result;
    }

    /**
     * 展平邮件原始数据。
     *
     * @param raw            邮件对象节点
     * @param recipientEmail 收件人地址
     * @return 归一化字段字典
     */
    private static Map<String, Object> flattenMail(JsonObject raw, String recipientEmail) {
        JsonElement headersEl = raw.get("headers");
        JsonObject headers = headersEl != null && headersEl.isJsonObject()
                ? headersEl.getAsJsonObject() : new JsonObject();

        String msgId = firstNonEmpty(
                Json.str(raw, "id"), Json.str(raw, "messageId"),
                Json.str(headers, "message-id"), Json.str(headers, "messageId"));
        if (msgId.isEmpty()) {
            msgId = String.join("\n",
                    Json.str(headers, "from"),
                    Json.str(headers, "subject"),
                    Json.str(headers, "date"),
                    recipientEmail);
        }

        Map<String, Object> flat = new LinkedHashMap<>();
        flat.put("id", msgId);
        flat.put("from", firstNonEmpty(Json.str(headers, "from"), Json.str(raw, "from")));
        flat.put("to", recipientEmail);
        flat.put("subject", firstNonEmpty(Json.str(headers, "subject"), Json.str(raw, "subject")));
        flat.put("text", Json.str(raw, "text"));
        flat.put("html", Json.str(raw, "html"));
        flat.put("date", firstNonEmpty(Json.str(headers, "date"), Json.str(raw, "date")));
        flat.put("isRead", false);
        JsonElement att = raw.get("attachments");
        if (att != null) {
            flat.put("attachments", Json.toRaw(att));
        }
        return flat;
    }

    private static String normalizeHost(String domain) {
        String raw = domain == null ? "" : domain.trim();
        if (raw.isEmpty()) return DEFAULT_HOST;
        String host = raw;
        String lower = host.toLowerCase();
        if (lower.startsWith("http://") || lower.startsWith("https://")) {
            host = host.split("://", 2)[1];
        }
        if (host.contains("@")) {
            String[] parts = host.split("@");
            host = parts[parts.length - 1];
        }
        if (host.contains("/")) {
            host = host.split("/", 2)[0];
        }
        host = host.replaceAll("^\\.+|\\.+$", "");
        return host.isEmpty() ? DEFAULT_HOST : host;
    }

    private static String firstNonEmpty(String... values) {
        for (String v : values) {
            if (v != null && !v.isEmpty()) return v;
        }
        return "";
    }
}
