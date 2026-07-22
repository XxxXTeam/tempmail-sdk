package com.xxxxteam.tempmail.providers;

import com.xxxxteam.tempmail.Config;
import com.xxxxteam.tempmail.SdkConfig;

import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Proxy;
import java.net.Socket;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.security.cert.X509Certificate;
import java.util.Base64;
import java.util.Map;

/**
 * 最小化 WebSocket 客户端（同步阻塞）。
 *
 * <p>支持 wss:// (TLS) 与 ws:// 两种协议，仅用于短时连接
 * （Socket.IO 握手 / 阻塞式收信）。自动处理 ping/pong 与 close 帧。</p>
 */
final class WebSocketConn {

    private final Socket socket;
    private final InputStream in;
    private final OutputStream out;
    private final SecureRandom random = new SecureRandom();

    private WebSocketConn(Socket socket, InputStream in, OutputStream out) {
        this.socket = socket;
        this.in = in;
        this.out = out;
    }

    /**
     * 连接到 WebSocket 服务端并完成升级握手。
     *
     * @param url       完整 WebSocket URL（wss://host/path?query）
     * @param headers   附加请求头，可为 null
     * @param timeoutSec 连接与读写超时（秒）
     * @return 已握手的连接实例
     */
    static WebSocketConn connect(String url, Map<String, String> headers, int timeoutSec) throws Exception {
        URI uri = URI.create(url);
        String scheme = uri.getScheme() == null ? "wss" : uri.getScheme();
        String host = uri.getHost();
        if (host == null) throw new IOException("WebSocket: invalid URL");
        int port = uri.getPort() > 0 ? uri.getPort() : ("wss".equals(scheme) ? 443 : 80);
        String path = (uri.getRawPath() == null || uri.getRawPath().isEmpty() ? "/" : uri.getRawPath())
                + (uri.getRawQuery() != null ? "?" + uri.getRawQuery() : "");

        SdkConfig cfg = Config.get();
        Socket socket = buildSocket(scheme, host, port, cfg, timeoutSec);
        socket.setSoTimeout(timeoutSec * 1000);
        InputStream in = socket.getInputStream();
        OutputStream out = socket.getOutputStream();

        byte[] keyBytes = new byte[16];
        new SecureRandom().nextBytes(keyBytes);
        String key = Base64.getEncoder().encodeToString(keyBytes);

        StringBuilder req = new StringBuilder();
        req.append("GET ").append(path).append(" HTTP/1.1\r\n");
        req.append("Host: ").append(host).append("\r\n");
        req.append("Upgrade: websocket\r\n");
        req.append("Connection: Upgrade\r\n");
        req.append("Sec-WebSocket-Key: ").append(key).append("\r\n");
        req.append("Sec-WebSocket-Version: 13\r\n");
        boolean hasOrigin = false;
        if (headers != null) {
            for (Map.Entry<String, String> kv : headers.entrySet()) {
                if ("Origin".equalsIgnoreCase(kv.getKey())) hasOrigin = true;
                req.append(kv.getKey()).append(": ").append(kv.getValue()).append("\r\n");
            }
        }
        if (!hasOrigin) req.append("Origin: https://").append(host).append("\r\n");
        req.append("\r\n");

        out.write(req.toString().getBytes(StandardCharsets.UTF_8));
        out.flush();

        StringBuilder resp = new StringBuilder();
        while (true) {
            int b = in.read();
            if (b < 0) throw new IOException("WebSocket: connection closed during handshake");
            resp.append((char) b);
            if (resp.length() >= 4 && "\r\n\r\n".equals(resp.substring(resp.length() - 4))) break;
        }
        if (!resp.toString().contains("101")) {
            throw new IOException("WebSocket: upgrade rejected");
        }
        return new WebSocketConn(socket, in, out);
    }

    private static Socket buildSocket(String scheme, String host, int port, SdkConfig cfg, int timeoutSec)
            throws Exception {
        Socket raw;
        String proxy = cfg.getProxy();
        if (proxy != null && !proxy.isBlank()) {
            URI u = URI.create(proxy.trim());
            int pport = u.getPort() > 0 ? u.getPort() : 80;
            raw = new Socket(new Proxy(Proxy.Type.HTTP, new InetSocketAddress(u.getHost(), pport)));
            raw.connect(new InetSocketAddress(host, port), timeoutSec * 1000);
        } else {
            raw = new Socket();
            raw.connect(new InetSocketAddress(host, port), timeoutSec * 1000);
        }

        if (!"wss".equals(scheme)) {
            return raw;
        }

        SSLContext sslCtx;
        if (cfg.isInsecure()) {
            TrustManager[] trustAll = {new X509TrustManager() {
                public void checkClientTrusted(X509Certificate[] c, String a) {}
                public void checkServerTrusted(X509Certificate[] c, String a) {}
                public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
            }};
            sslCtx = SSLContext.getInstance("TLS");
            sslCtx.init(null, trustAll, new SecureRandom());
        } else {
            sslCtx = SSLContext.getDefault();
        }
        SSLSocket ssl = (SSLSocket) sslCtx.getSocketFactory().createSocket(raw, host, port, true);
        ssl.setUseClientMode(true);
        ssl.startHandshake();
        return ssl;
    }

    /**
     * 发送文本帧（客户端必须 mask）。
     *
     * @param text 文本内容
     */
    void send(String text) throws IOException {
        byte[] payload = text.getBytes(StandardCharsets.UTF_8);
        int len = payload.length;
        byte[] mask = new byte[4];
        random.nextBytes(mask);

        ByteArrayOutputStream buf = new ByteArrayOutputStream();
        buf.write(0x81);
        if (len < 126) {
            buf.write(len | 0x80);
        } else if (len < 65536) {
            buf.write(126 | 0x80);
            buf.write((len >> 8) & 0xFF);
            buf.write(len & 0xFF);
        } else {
            buf.write(127 | 0x80);
            for (int i = 7; i >= 0; i--) buf.write((int) ((len >> (i * 8)) & 0xFF));
        }
        buf.write(mask);
        for (int i = 0; i < len; i++) buf.write(payload[i] ^ mask[i % 4]);
        out.write(buf.toByteArray());
        out.flush();
    }

    /**
     * 接收一条文本消息（阻塞）。超时或连接关闭返回 null，自动处理 ping/pong 与 close。
     *
     * @param timeoutMs 超时毫秒
     * @return 文本内容，超时/关闭返回 null
     */
    String receive(int timeoutMs) throws IOException {
        if (timeoutMs > 0) socket.setSoTimeout(timeoutMs);
        try {
            int b1 = in.read();
            if (b1 < 0) return null;
            int b2 = in.read();
            if (b2 < 0) return null;

            int opcode = b1 & 0x0F;
            boolean masked = (b2 & 0x80) != 0;
            long payloadLen = b2 & 0x7F;
            if (payloadLen == 126) {
                payloadLen = ((in.read() & 0xFF) << 8) | (in.read() & 0xFF);
            } else if (payloadLen == 127) {
                payloadLen = 0;
                for (int i = 0; i < 8; i++) payloadLen = (payloadLen << 8) | (in.read() & 0xFF);
            }
            byte[] maskKey = masked ? in.readNBytes(4) : null;
            byte[] payload = in.readNBytes((int) payloadLen);
            if (maskKey != null) {
                for (int i = 0; i < payload.length; i++) payload[i] ^= maskKey[i % 4];
            }
            switch (opcode) {
                case 0x01:
                case 0x02:
                    return new String(payload, StandardCharsets.UTF_8);
                case 0x08:
                    close();
                    return null;
                case 0x09:
                    sendPong(payload);
                    return receive(timeoutMs);
                default:
                    return receive(timeoutMs);
            }
        } catch (java.net.SocketTimeoutException e) {
            return null;
        }
    }

    private void sendPong(byte[] payload) throws IOException {
        ByteArrayOutputStream buf = new ByteArrayOutputStream();
        buf.write(0x8A);
        buf.write(payload.length | 0x80);
        byte[] mask = new byte[4];
        random.nextBytes(mask);
        buf.write(mask);
        for (int i = 0; i < payload.length; i++) buf.write(payload[i] ^ mask[i % 4]);
        out.write(buf.toByteArray());
        out.flush();
    }

    /**
     * 设置底层读超时（毫秒）。
     *
     * @param timeoutMs 超时毫秒
     */
    void setTimeout(int timeoutMs) throws java.net.SocketException {
        socket.setSoTimeout(Math.max(1, timeoutMs));
    }

    /**
     * 关闭连接。
     */
    void close() {
        try {
            socket.close();
        } catch (IOException ignored) {
        }
    }
}
