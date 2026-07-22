package com.xxxxteam.tempmail;

/**
 * 全局配置管理器。
 *
 * <p>设置配置后自动递增版本号，使共享 {@link HttpClient} 失效并按新配置重建。</p>
 */
public final class Config {

    private static final Object LOCK = new Object();
    // 读多写极少：用 volatile 支持热路径无锁读（每个 HTTP 请求都会读取），写入时加锁保证 version 递增与 global 赋值的顺序
    private static volatile SdkConfig global = loadEnvConfig();
    private static volatile int version;

    private Config() {
    }

    /**
     * 从环境变量读取默认配置。
     *
     * @return 初始化后的配置对象
     */
    private static SdkConfig loadEnvConfig() {
        SdkConfig cfg = new SdkConfig();

        String proxy = System.getenv("TEMPMAIL_PROXY");
        if (proxy != null && !proxy.isBlank()) {
            cfg.setProxy(proxy);
        }

        String timeoutStr = System.getenv("TEMPMAIL_TIMEOUT");
        if (timeoutStr != null) {
            try {
                int t = Integer.parseInt(timeoutStr.trim());
                if (t > 0) {
                    cfg.setTimeout(t);
                }
            } catch (NumberFormatException ignored) {
                // 非法值忽略，沿用默认超时
            }
        }

        String insecure = System.getenv("TEMPMAIL_INSECURE");
        if (insecure != null) {
            String v = insecure.trim();
            cfg.setInsecure("1".equals(v) || "true".equalsIgnoreCase(v));
        }

        String dmTok = System.getenv("DROPMAIL_AUTH_TOKEN");
        if (dmTok == null || dmTok.isBlank()) {
            dmTok = System.getenv("DROPMAIL_API_TOKEN");
        }
        if (dmTok != null && !dmTok.isBlank()) {
            cfg.setDropmailAuthToken(dmTok.trim());
        }

        String apihzId = System.getenv("APIHZ_ID");
        if (apihzId != null && !apihzId.isBlank()) {
            cfg.setApihzId(apihzId.trim());
        }
        String apihzKey = System.getenv("APIHZ_KEY");
        if (apihzKey != null && !apihzKey.isBlank()) {
            cfg.setApihzKey(apihzKey.trim());
        }

        String te = System.getenv("TEMPMAIL_TELEMETRY_ENABLED");
        if (te != null) {
            String v = te.trim().toLowerCase();
            if ("false".equals(v) || "0".equals(v) || "no".equals(v)) {
                cfg.setTelemetryEnabled(Boolean.FALSE);
            } else if ("true".equals(v) || "1".equals(v) || "yes".equals(v)) {
                cfg.setTelemetryEnabled(Boolean.TRUE);
            }
        }

        String tu = System.getenv("TEMPMAIL_TELEMETRY_URL");
        if (tu != null && !tu.isBlank()) {
            cfg.setTelemetryUrl(tu.trim());
        }

        return cfg;
    }

    /**
     * 设置 SDK 全局配置，触发共享 HttpClient 失效重建。
     *
     * @param config 新配置，null 时重置为默认
     */
    public static void set(SdkConfig config) {
        synchronized (LOCK) {
            global = config != null ? config : new SdkConfig();
            version++;
        }
    }

    /**
     * 获取当前 SDK 全局配置。
     *
     * @return 当前配置
     */
    public static SdkConfig get() {
        // volatile 读，无需加锁
        return global;
    }

    /**
     * 获取当前配置版本号，用于缓存失效判断。
     *
     * @return 版本号
     */
    public static int getVersion() {
        // volatile 读，无需加锁
        return version;
    }
}
