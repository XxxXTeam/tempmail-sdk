package com.xxxxteam.tempmail;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.regex.Pattern;

/**
 * 匿名用量遥测。默认开启，可通过配置或环境变量关闭。
 *
 * <p>上报内容仅含操作类型、渠道、成功与否、尝试次数等，邮箱地址会被脱敏。
 * {@code sdk_language} 固定为 {@code java}。</p>
 */
public final class Telemetry {

    private static final String DEFAULT_URL = "https://sdk-1.openel.top/v1/event";
    private static final int MAX_BATCH = 32;
    private static final long FLUSH_SEC = 2L;
    private static final String SDK_VERSION = "0.1.0";

    private static final Pattern EMAIL_RE = Pattern.compile("[^\\s@]{1,64}@[^\\s@]{1,255}");
    private static final ConcurrentLinkedQueue<Map<String, Object>> QUEUE = new ConcurrentLinkedQueue<>();
    private static final AtomicBoolean PERIODIC_STARTED = new AtomicBoolean(false);
    private static final ScheduledExecutorService SCHEDULER = Executors.newSingleThreadScheduledExecutor(r -> {
        Thread t = new Thread(r, "tempmail-telemetry");
        t.setDaemon(true);
        return t;
    });

    private Telemetry() {
    }

    private static boolean isOn() {
        Boolean v = Config.get().getTelemetryEnabled();
        return v == null || v;
    }

    private static String resolveUrl() {
        String u = Config.get().getTelemetryUrl();
        u = u != null ? u.trim() : "";
        return u.isEmpty() ? DEFAULT_URL : u;
    }

    private static String sanitize(String msg) {
        if (msg == null || msg.isEmpty()) {
            return "";
        }
        String redacted = EMAIL_RE.matcher(msg).replaceAll("[redacted]");
        return redacted.length() > 400 ? redacted.substring(0, 400) : redacted;
    }

    /**
     * 上报一条遥测事件（非阻塞，失败静默）。
     *
     * @param operation    操作类型
     * @param channel      渠道标识
     * @param success      是否成功
     * @param attemptCount 尝试次数
     * @param channelsTried 已尝试渠道数
     * @param error        错误信息
     */
    public static void report(String operation, String channel, boolean success,
                              int attemptCount, int channelsTried, String error) {
        if (!isOn()) {
            return;
        }
        ensurePeriodic();

        Map<String, Object> ev = new LinkedHashMap<>();
        ev.put("operation", operation);
        ev.put("channel", channel);
        ev.put("success", success);
        ev.put("attempt_count", attemptCount);
        ev.put("ts_ms", System.currentTimeMillis());
        if (channelsTried > 0) {
            ev.put("channels_tried", channelsTried);
        }
        String err = sanitize(error);
        if (!err.isEmpty()) {
            ev.put("error", err);
        }

        QUEUE.add(ev);
        if (QUEUE.size() >= MAX_BATCH) {
            flushBatch();
        }
    }

    private static void ensurePeriodic() {
        if (PERIODIC_STARTED.compareAndSet(false, true)) {
            SCHEDULER.scheduleWithFixedDelay(Telemetry::flushBatch, FLUSH_SEC, FLUSH_SEC, TimeUnit.SECONDS);
        }
    }

    private static void flushBatch() {
        if (!isOn()) {
            QUEUE.clear();
            return;
        }

        List<Map<String, Object>> events = new ArrayList<>();
        Map<String, Object> ev;
        while ((ev = QUEUE.poll()) != null) {
            events.add(ev);
        }
        if (events.isEmpty()) {
            return;
        }

        Map<String, Object> batch = new LinkedHashMap<>();
        batch.put("schema_version", 2);
        batch.put("sdk_language", "java");
        batch.put("sdk_version", SDK_VERSION);
        batch.put("os", osName());
        batch.put("arch", System.getProperty("os.arch", "unknown").toLowerCase());
        batch.put("events", events);

        String body = Json.serialize(batch);
        String url = resolveUrl();
        SCHEDULER.execute(() -> {
            try {
                Map<String, String> headers = new LinkedHashMap<>();
                headers.put("User-Agent", "tempmail-sdk-java/" + SDK_VERSION);
                HttpClient.send("POST", url, headers, body, "application/json; charset=utf-8", 8);
            } catch (RuntimeException ignored) {
                // 遥测失败静默
            }
        });
    }

    private static String osName() {
        String r = System.getProperty("os.name", "").toLowerCase();
        if (r.contains("windows")) {
            return "windows";
        }
        if (r.contains("linux")) {
            return "linux";
        }
        if (r.contains("mac") || r.contains("darwin")) {
            return "darwin";
        }
        return "unknown";
    }
}
