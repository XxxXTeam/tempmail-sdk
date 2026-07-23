package com.xxxxteam.tempmail.providers;

import com.xxxxteam.tempmail.Email;
import com.xxxxteam.tempmail.EmailInfo;
import com.xxxxteam.tempmail.HttpClient;
import com.xxxxteam.tempmail.HttpResult;
import com.xxxxteam.tempmail.Json;
import com.xxxxteam.tempmail.Normalizer;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * vip-215 渠道实现（vip.215.im）。
 *
 * <p>generate: GET 首页获取 cookie → POST /api/temp-inbox 创建收件箱；
 * getEmails: GET /v1/auth/ws-ticket 获取 ticket → WebSocket /v1/ws?token={ticket}
 * 监听 message.new 事件（阻塞 5 秒）。</p>
 */
public final class Vip215 {

    private static final String CHANNEL = "vip-215";
    private static final String BASE = "https://vip.215.im";
    private static final String USER_AGENT =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36 Edg/148.0.0.0";

    /** 收信阻塞等待时间（毫秒）。 */
    private static final int GET_EMAILS_WAIT_MS = 5000;

    private static final String SYNTHETIC_MARKER = "【tempmail-sdk|synthetic|vip-215|v1】";

    private Vip215() {
    }

    private static Map<String, String> apiHeaders() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", USER_AGENT);
        h.put("Accept", "*/*");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6");
        h.put("Cache-Control", "no-cache");
        h.put("Content-Type", "application/json");
        h.put("DNT", "1");
        h.put("Origin", BASE);
        h.put("Pragma", "no-cache");
        h.put("Referer", BASE + "/");
        h.put("Sec-CH-UA", "\"Chromium\";v=\"148\", \"Microsoft Edge\";v=\"148\", \"Not/A)Brand\";v=\"99\"");
        h.put("Sec-CH-UA-Mobile", "?0");
        h.put("Sec-CH-UA-Platform", "\"Windows\"");
        h.put("Sec-Fetch-Dest", "empty");
        h.put("Sec-Fetch-Mode", "cors");
        h.put("Sec-Fetch-Site", "same-origin");
        h.put("X-Locale", "zh");
        return h;
    }

    /**
     * HTTP POST 创建临时收件箱。
     *
     * @return 邮箱信息（token 为 JWT）
     */
    public static EmailInfo generate() {
        String cookie = establishSession();

        Map<String, String> headers = apiHeaders();
        headers.put("Cookie", cookie);
        HttpResult resp = HttpClient.post(BASE + "/api/temp-inbox", "", "application/json", headers);
        if (resp.getStatusCode() >= 400) {
            throw new RuntimeException(CHANNEL + ": create inbox failed: HTTP " + resp.getStatusCode());
        }

        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null || !body.has("success") || !body.get("success").getAsBoolean()) {
            throw new RuntimeException(CHANNEL + ": success=false");
        }

        JsonElement dataEl = body.get("data");
        if (dataEl == null || !dataEl.isJsonObject()) {
            throw new RuntimeException(CHANNEL + ": missing data");
        }
        JsonObject data = dataEl.getAsJsonObject();
        String address = Json.str(data, "address");
        String token = Json.str(data, "token");
        if (address.isEmpty() || token.isEmpty()) {
            throw new RuntimeException(CHANNEL + ": missing address or token");
        }
        String createdAt = Json.str(data, "createdAt");
        return new EmailInfo(CHANNEL, address, token, null, createdAt.isEmpty() ? null : createdAt);
    }

    /**
     * 通过 HTTP 接口获取邮件列表。
     * GET https://vip.215.im/v1/messages
     * 返回完整邮件（含真实正文），相比 WebSocket 推送的合成卡片更完整。
     *
     * @param jwt JWT 令牌
     * @return 邮件列表；HTTP 失败时抛异常
     */
    private static List<Map<String, Object>> fetchMessagesViaHttp(String jwt) {
        Map<String, String> headers = apiHeaders();
        headers.put("Authorization", "Bearer " + jwt);
        HttpResult resp = HttpClient.get(BASE + "/v1/messages", headers);
        if (resp.getStatusCode() < 200 || resp.getStatusCode() >= 300) {
            throw new RuntimeException(CHANNEL + ": /v1/messages HTTP " + resp.getStatusCode());
        }
        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null || !body.has("success") || !body.get("success").getAsBoolean()) {
            throw new RuntimeException(CHANNEL + ": /v1/messages success=false");
        }
        JsonElement dataEl = body.get("data");
        if (dataEl == null || !dataEl.isJsonObject()) return new ArrayList<>();
        JsonElement msgs = dataEl.getAsJsonObject().get("messages");
        List<Map<String, Object>> out = new ArrayList<>();
        if (msgs == null || !msgs.isJsonArray()) return out;
        for (JsonElement item : msgs.getAsJsonArray()) {
            if (item.isJsonObject()) {
                out.add(Json.toDict(item));
            }
        }
        return out;
    }

    /**
     * 通过 HTTP 详情接口获取单封邮件完整内容。
     * GET https://vip.215.im/v1/messages/{id}
     *
     * @param jwt JWT 令牌
     * @param id  邮件 ID
     * @return 详情 Map（含 text/html），失败返回 null
     */
    private static Map<String, Object> fetchMessageDetail(String jwt, String id) {
        if (id == null || id.isBlank()) return null;
        try {
            Map<String, String> headers = apiHeaders();
            headers.put("Authorization", "Bearer " + jwt);
            HttpResult resp = HttpClient.get(
                    BASE + "/v1/messages/" + urlEncode(id), headers);
            if (resp.getStatusCode() < 200 || resp.getStatusCode() >= 300) return null;
            JsonObject body = Json.parseObject(resp.getBody());
            if (body == null || !body.has("success") || !body.get("success").getAsBoolean()) return null;
            JsonElement d = body.get("data");
            if (d == null || !d.isJsonObject()) return null;
            return Json.toDict(d);
        } catch (RuntimeException e) {
            return null;
        }
    }

    /**
     * 判断行是否包含真实正文（用于决定是否补拉详情）。
     */
    private static boolean hasRealBody(Map<String, Object> row) {
        for (String key : new String[]{"text", "body_text", "html", "body_html", "content", "body"}) {
            Object v = row.get(key);
            if (v instanceof String s && !s.isBlank()) return true;
        }
        return false;
    }

    /**
     * 提取邮件 ID。
     */
    private static String extractMessageId(Map<String, Object> row) {
        for (String key : new String[]{"id", "messageId", "message_id"}) {
            Object v = row.get(key);
            if (v instanceof String s && !s.isBlank()) return s.trim();
            if (v instanceof Number n) return String.valueOf(n.longValue());
        }
        return "";
    }

    /**
     * 获取邮件列表（HTTP 优先，WebSocket 回退）。
     * 1. HTTP GET /v1/messages 拉取完整邮件（含真实正文）
     * 2. 缺正文的条目通过 GET /v1/messages/{id} 补拉详情
     * 3. HTTP 失败时回退到原 WebSocket 阻塞收信路径（合成卡片）
     *
     * @param email 邮箱地址
     * @param token JWT 令牌
     * @return 邮件列表
     */
    public static List<Email> getEmails(String email, String token) {
        if (token == null || token.isEmpty()) {
            throw new IllegalArgumentException(CHANNEL + ": token 不能为空");
        }

        /* 优先通过 HTTP 拉取完整邮件 */
        try {
            List<Map<String, Object>> messages = fetchMessagesViaHttp(token);
            List<Email> out = new ArrayList<>();
            for (Map<String, Object> row : messages) {
                String id = extractMessageId(row);
                /* 若无真实正文，补拉详情 */
                if (!hasRealBody(row) && !id.isEmpty()) {
                    Map<String, Object> detail = fetchMessageDetail(token, id);
                    if (detail != null) {
                        for (Map.Entry<String, Object> e : detail.entrySet()) {
                            if (e.getValue() != null) row.put(e.getKey(), e.getValue());
                        }
                    }
                }
                if (!row.containsKey("to")) row.put("to", email);
                out.add(Normalizer.normalizeEmail(row, email));
            }
            return out;
        } catch (RuntimeException httpErr) {
            /* HTTP 失败时回退到 WebSocket 阻塞收信（合成卡片） */
            return getEmailsViaWebSocket(email, token);
        }
    }

    /**
     * WebSocket 阻塞式收信：连接后监听 message.new 事件（回退路径）。
     *
     * @param email 邮箱地址
     * @param token JWT 令牌
     * @return 邮件列表
     */
    private static List<Email> getEmailsViaWebSocket(String email, String token) {
        String ticket = fetchWsTicket(token);
        String wsUrl = "wss://vip.215.im/v1/ws?token=" + urlEncode(ticket);

        Map<String, String> wsHeaders = new LinkedHashMap<>();
        wsHeaders.put("User-Agent", USER_AGENT);
        wsHeaders.put("Origin", BASE);

        WebSocketConn ws = null;
        try {
            ws = WebSocketConn.connect(wsUrl, wsHeaders, 15);
            List<Email> result = new ArrayList<>();
            long deadline = System.currentTimeMillis() + GET_EMAILS_WAIT_MS;

            while (true) {
                int remaining = (int) (deadline - System.currentTimeMillis());
                if (remaining <= 0) break;
                String msg = ws.receive(remaining);
                if (msg == null) break;

                JsonObject parsed = Json.parseObject(msg);
                if (parsed == null) continue;
                if (!"message.new".equals(Json.str(parsed, "type"))) continue;

                JsonElement dataEl = parsed.get("data");
                if (dataEl == null || !dataEl.isJsonObject()) continue;
                JsonObject data = dataEl.getAsJsonObject();

                String[] bodies = buildSyntheticBodies(email, data);
                Map<String, Object> flat = new LinkedHashMap<>();
                flat.put("id", Json.str(data, "id"));
                flat.put("from", Json.str(data, "from"));
                flat.put("subject", Json.str(data, "subject"));
                flat.put("date", Json.str(data, "date"));
                flat.put("to", email);
                flat.put("text", bodies[0]);
                flat.put("html", bodies[1]);

                Email em = Normalizer.normalizeEmail(flat, email);
                if (em.getId() != null && !em.getId().isEmpty()) {
                    result.add(em);
                }
            }
            return result;
        } catch (RuntimeException e) {
            throw e;
        } catch (Exception e) {
            throw new RuntimeException(CHANNEL + ": getEmails 失败: " + e.getMessage(), e);
        } finally {
            if (ws != null) ws.close();
        }
    }

    /**
     * 获取首页 cookie（yyds_homepage_bridge + yyds_homepage_device）。
     */
    private static String establishSession() {
        Map<String, String> h = new LinkedHashMap<>();
        h.put("User-Agent", USER_AGENT);
        h.put("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8");
        h.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6");
        h.put("Cache-Control", "no-cache");
        h.put("DNT", "1");
        h.put("Pragma", "no-cache");

        HttpResult resp = HttpClient.get(BASE + "/", h);
        if (resp.getStatusCode() >= 400) {
            throw new RuntimeException(CHANNEL + ": homepage failed");
        }

        List<String> parts = new ArrayList<>();
        for (String sc : resp.getSetCookies()) {
            int semi = sc.indexOf(';');
            String nv = semi > 0 ? sc.substring(0, semi) : sc;
            if (nv.contains("=")) parts.add(nv.trim());
        }
        String cookie = String.join("; ", parts);
        if (!cookie.contains("yyds_homepage_bridge=") || !cookie.contains("yyds_homepage_device=")) {
            throw new RuntimeException(CHANNEL + ": missing homepage cookies");
        }
        return cookie;
    }

    /**
     * 获取 WebSocket ticket。
     *
     * @param jwt JWT 令牌
     * @return ticket 字符串
     */
    private static String fetchWsTicket(String jwt) {
        Map<String, String> headers = apiHeaders();
        headers.put("Authorization", "Bearer " + jwt);
        HttpResult resp = HttpClient.get(BASE + "/v1/auth/ws-ticket", headers);
        if (resp.getStatusCode() >= 400) {
            throw new RuntimeException(CHANNEL + ": ws-ticket request failed");
        }
        JsonObject body = Json.parseObject(resp.getBody());
        if (body == null || !body.has("success") || !body.get("success").getAsBoolean()) {
            throw new RuntimeException(CHANNEL + ": ws-ticket success=false");
        }
        JsonElement dataEl = body.get("data");
        String ticket = dataEl != null && dataEl.isJsonObject()
                ? Json.str(dataEl.getAsJsonObject(), "ticket") : "";
        if (ticket.isEmpty()) {
            throw new RuntimeException(CHANNEL + ": missing ws ticket");
        }
        return ticket;
    }

    /**
     * 构建合成正文（WebSocket 推送无完整 body，用元数据摘要填充）。
     *
     * @param recipientEmail 收件人地址
     * @param data           message.new 的 data 对象
     * @return [text, html]
     */
    private static String[] buildSyntheticBodies(String recipientEmail, JsonObject data) {
        String id      = sanitize(Json.str(data, "id"));
        String subject = sanitize(Json.str(data, "subject"));
        String from    = sanitize(Json.str(data, "from"));
        String to      = sanitize(recipientEmail);
        String date    = sanitize(Json.str(data, "date"));

        String text = SYNTHETIC_MARKER + "\n"
                + "id: " + id + "\n"
                + "subject: " + subject + "\n"
                + "from: " + from + "\n"
                + "to: " + to + "\n"
                + "date: " + date;

        String html = "<div class=\"tempmail-sdk-synthetic\" data-tempmail-sdk-format=\"synthetic-v1\" data-channel=\"vip-215\">"
                + "<dl class=\"tempmail-sdk-meta\">"
                + "<dt>id</dt><dd>" + escHtml(id) + "</dd>"
                + "<dt>subject</dt><dd>" + escHtml(subject) + "</dd>"
                + "<dt>from</dt><dd>" + escHtml(from) + "</dd>"
                + "<dt>to</dt><dd>" + escHtml(to) + "</dd>"
                + "<dt>date</dt><dd>" + escHtml(date) + "</dd>"
                + "</dl></div>";

        return new String[]{text, html};
    }

    private static String sanitize(String val) {
        if (val == null) return "";
        return val.replace("\r\n", " ").replace("\r", " ").replace("\n", " ").trim();
    }

    private static String escHtml(String s) {
        return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;");
    }

    private static String urlEncode(String s) {
        try {
            return java.net.URLEncoder.encode(s, java.nio.charset.StandardCharsets.UTF_8);
        } catch (RuntimeException e) {
            return s;
        }
    }
}
