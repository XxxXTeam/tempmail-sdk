package com.xxxxteam.tempmail.providers;

import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Socket.IO 临时邮箱共享实现。
 *
 * <p>用于 mjj.cm、linshi.co 等使用相同 Socket.IO 协议的站点。
 * generate: 连接 → request shortid → 接收 shortid → 断开；
 * getEmails: 连接 → set shortid → 阻塞监听 mail 事件 → 收集后断开。</p>
 */
final class SocketIoMail {

    /** 收信阻塞等待时间（毫秒）。 */
    private static final int GET_EMAILS_WAIT_MS = 5000;

    private SocketIoMail() {
    }

    /**
     * 请求 shortid 并生成邮箱。
     *
     * @param channel 渠道标识
     * @param host    主机名
     * @return 邮箱信息
     */
    static EmailInfo generate(String channel, String host) {
        SocketIoClient sio = null;
        try {
            sio = SocketIoClient.connect(host, 15);
            sio.emit("request shortid", "true");
            String payload = sio.waitForEvent("shortid", 15000);
            String shortid = SocketIoClient.jsonStringValue(payload).trim();
            if (shortid.isEmpty()) {
                throw new RuntimeException(channel + ": shortid 为空");
            }
            return new EmailInfo(channel, shortid + "@" + host);
        } catch (RuntimeException e) {
            throw e;
        } catch (Exception e) {
            throw new RuntimeException(channel + ": generate 失败: " + e.getMessage(), e);
        } finally {
            if (sio != null) sio.close();
        }
    }

    /**
     * 阻塞式收信：连接后订阅 shortid，等待 mail 事件。
     *
     * @param channel 渠道标识
     * @param email   邮箱地址
     * @param token   认证令牌（未使用）
     * @return 邮件列表
     */
    static List<Email> getEmails(String channel, String email, String token) {
        String trimmed = email == null ? "" : email.trim();
        int atPos = trimmed.indexOf('@');
        if (atPos <= 0) {
            throw new IllegalArgumentException(channel + ": invalid email address");
        }
        String local = trimmed.substring(0, atPos);
        String host = trimmed.substring(atPos + 1);
        if (host.isEmpty()) host = channel;

        SocketIoClient sio = null;
        try {
            sio = SocketIoClient.connect(host, 15);
            sio.emit("set shortid", "\"" + local + "\"");

            List<Email> result = new ArrayList<>();
            long deadline = System.currentTimeMillis() + GET_EMAILS_WAIT_MS;
            while (true) {
                int remaining = (int) (deadline - System.currentTimeMillis());
                if (remaining <= 0) break;
                String[] event = sio.receiveEvent(remaining);
                if (event == null) break;
                if ("mail".equals(event[0])) {
                    JsonElement payload = Json.parse(event[1]);
                    if (payload != null && payload.isJsonObject()) {
                        Map<String, Object> flat = flattenMail(payload.getAsJsonObject(), trimmed);
                        result.add(Normalizer.normalizeEmail(flat, trimmed));
                    }
                }
            }
            return result;
        } catch (RuntimeException e) {
            throw e;
        } catch (Exception e) {
            throw new RuntimeException(channel + ": getEmails 失败: " + e.getMessage(), e);
        } finally {
            if (sio != null) sio.close();
        }
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
                    firstNonEmpty(Json.str(headers, "from"), Json.str(raw, "from")),
                    firstNonEmpty(Json.str(headers, "subject"), Json.str(raw, "subject")),
                    firstNonEmpty(Json.str(headers, "date"), Json.str(raw, "date")),
                    recipientEmail);
        }

        Map<String, Object> flat = new LinkedHashMap<>();
        flat.put("id", msgId);
        flat.put("from", firstNonEmpty(Json.str(headers, "from"), Json.str(raw, "from")));
        flat.put("to", recipientEmail);
        flat.put("subject", firstNonEmpty(Json.str(headers, "subject"), Json.str(raw, "subject")));
        flat.put("text", firstNonEmpty(Json.str(raw, "text"), Json.str(raw, "body")));
        flat.put("html", Json.str(raw, "html"));
        flat.put("date", firstNonEmpty(Json.str(headers, "date"), Json.str(raw, "date")));
        flat.put("isRead", false);
        return flat;
    }

    private static String firstNonEmpty(String... values) {
        for (String v : values) {
            if (v != null && !v.isEmpty()) return v;
        }
        return "";
    }
}
