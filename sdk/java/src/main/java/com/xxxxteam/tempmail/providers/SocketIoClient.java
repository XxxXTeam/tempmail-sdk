package com.xxxxteam.tempmail.providers;

import java.io.IOException;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Socket.IO 协议辅助层（Engine.IO v3/v4，WebSocket 传输）。
 *
 * <p>在 {@link WebSocketConn} 之上封装 Socket.IO 握手与事件收发。
 * 仅支持短时同步操作（generate / 阻塞式 getEmails）。</p>
 */
final class SocketIoClient {

    private static final String USER_AGENT =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0";

    private static final int[] EIO_VERSIONS = {4, 3};

    private final WebSocketConn ws;

    private SocketIoClient(WebSocketConn ws) {
        this.ws = ws;
    }

    /**
     * 连接到 Socket.IO 服务端（自动尝试 EIO v4/v3）。
     *
     * @param host    主机名（如 mjj.cm）
     * @param timeout 超时秒数
     * @return 已握手的客户端实例
     */
    static SocketIoClient connect(String host, int timeout) throws IOException {
        Exception lastErr = null;
        for (int eio : EIO_VERSIONS) {
            try {
                String url = "wss://" + host + "/socket.io/?EIO=" + eio + "&transport=websocket";
                Map<String, String> headers = new LinkedHashMap<>();
                headers.put("User-Agent", USER_AGENT);
                headers.put("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
                headers.put("Cache-Control", "no-cache");
                headers.put("DNT", "1");
                headers.put("Pragma", "no-cache");
                headers.put("Origin", "https://" + host);

                WebSocketConn ws = WebSocketConn.connect(url, headers, timeout);

                // 等待 Engine.IO open 包（以 "0" 开头）
                String packet = ws.receive(timeout * 1000);
                if (packet == null || !packet.startsWith("0")) {
                    ws.close();
                    throw new IOException("unexpected open packet for EIO=" + eio);
                }

                // 发送 Socket.IO connect
                ws.send("40");
                return new SocketIoClient(ws);
            } catch (Exception e) {
                lastErr = e;
            }
        }
        throw new IOException("SocketIO: handshake failed: "
                + (lastErr != null ? lastErr.getMessage() : "unknown"), lastErr);
    }

    /**
     * 发送 Socket.IO 事件。
     *
     * @param event      事件名
     * @param payloadJson 载荷 JSON 文本（已序列化）
     */
    void emit(String event, String payloadJson) throws IOException {
        ws.send("42[\"" + event + "\"," + payloadJson + "]");
    }

    /**
     * 等待指定事件名的响应，返回载荷 JSON 文本。超时抛出异常。
     *
     * @param eventName 事件名
     * @param timeoutMs 超时毫秒
     * @return 载荷 JSON 文本
     */
    String waitForEvent(String eventName, int timeoutMs) throws IOException {
        long deadline = System.currentTimeMillis() + timeoutMs;
        while (true) {
            int remaining = (int) (deadline - System.currentTimeMillis());
            if (remaining <= 0) {
                throw new IOException("SocketIO: timeout waiting for event '" + eventName + "'");
            }
            String packet = ws.receive(remaining);
            if (packet == null) {
                throw new IOException("SocketIO: connection closed waiting for '" + eventName + "'");
            }
            if ("2".equals(packet)) { ws.send("3"); continue; }
            if (packet.startsWith("40")) continue;
            if (packet.startsWith("42")) {
                String[] parsed = parseEventArray(packet.substring(2));
                if (parsed != null && eventName.equals(parsed[0])) {
                    return parsed[1];
                }
            }
        }
    }

    /**
     * 接收下一个 Socket.IO 事件，返回 [eventName, payloadJson]，超时返回 null。
     *
     * @param timeoutMs 超时毫秒
     * @return [eventName, payloadJson] 或 null
     */
    String[] receiveEvent(int timeoutMs) throws IOException {
        long deadline = System.currentTimeMillis() + timeoutMs;
        while (true) {
            int remaining = (int) (deadline - System.currentTimeMillis());
            if (remaining <= 0) return null;
            String packet = ws.receive(remaining);
            if (packet == null) return null;
            if ("2".equals(packet)) { ws.send("3"); continue; }
            if (packet.startsWith("40")) continue;
            if (packet.startsWith("42")) {
                String[] parsed = parseEventArray(packet.substring(2));
                if (parsed != null) return parsed;
            }
        }
    }

    void close() {
        ws.close();
    }

    // ---- 基于 Gson 的事件数组解析 ----

    /**
     * 解析 Socket.IO 事件数组 ["eventName", payload]，返回 [eventName, payloadJson]。
     */
    private static String[] parseEventArray(String json) {
        try {
            com.google.gson.JsonElement el = com.google.gson.JsonParser.parseString(json);
            if (!el.isJsonArray()) return null;
            com.google.gson.JsonArray arr = el.getAsJsonArray();
            if (arr.isEmpty() || !arr.get(0).isJsonPrimitive()) return null;
            String eventName = arr.get(0).getAsString();
            String payloadJson = arr.size() > 1 ? arr.get(1).toString() : "null";
            return new String[]{eventName, payloadJson};
        } catch (RuntimeException e) {
            return null;
        }
    }

    /**
     * 从载荷 JSON 文本中提取纯字符串值（用于 mailbox / shortid 等字符串事件）。
     *
     * @param payloadJson 载荷 JSON 文本
     * @return 纯字符串，非字符串或解析失败返回空串
     */
    static String jsonStringValue(String payloadJson) {
        try {
            com.google.gson.JsonElement el = com.google.gson.JsonParser.parseString(payloadJson);
            if (el.isJsonPrimitive()) return el.getAsString();
        } catch (RuntimeException ignored) {
        }
        return "";
    }
}
