package com.xxxxteam.tempmail;

import java.util.Map;

/**
 * SDK 全局配置：代理、超时、TLS、遥测等，作用于所有 HTTP 请求。
 *
 * <p>支持环境变量自动读取（优先级低于代码显式设置）：</p>
 * <ul>
 *   <li>{@code TEMPMAIL_PROXY} —— 代理 URL（http/https/socks5）</li>
 *   <li>{@code TEMPMAIL_TIMEOUT} —— 超时秒数</li>
 *   <li>{@code TEMPMAIL_INSECURE} —— {@code 1}/{@code true} 跳过 TLS 验证</li>
 *   <li>{@code DROPMAIL_AUTH_TOKEN} —— DropMail GraphQL af_ 令牌</li>
 *   <li>{@code APIHZ_ID} / {@code APIHZ_KEY} —— apihz（接口盒子）调用凭据</li>
 *   <li>{@code TEMPMAIL_TELEMETRY_ENABLED} —— {@code false}/{@code 0}/{@code no} 关闭匿名用量上报</li>
 *   <li>{@code TEMPMAIL_TELEMETRY_URL} —— 自定义上报端点</li>
 * </ul>
 */
public final class SdkConfig {

    /** 代理 URL，支持 http/https/socks5，如 "http://127.0.0.1:7890"，可为 null。 */
    private String proxy;

    /** 全局默认超时秒数。 */
    private int timeout = 15;

    /** 跳过 TLS 证书验证（调试用）。 */
    private boolean insecure;

    /** 自定义请求头，会合并到每个请求中，可为 null。 */
    private Map<String, String> headers;

    /** DropMail GraphQL 路径中的 af_ 令牌；空则自动申请。 */
    private String dropmailAuthToken;

    /** apihz 调用 id；空则使用官方公共账号。 */
    private String apihzId;

    /** apihz 调用 key；空则使用官方公共账号。 */
    private String apihzKey;

    /** null 表示默认开启匿名上报；FALSE 关闭；TRUE 显式开启。 */
    private Boolean telemetryEnabled;

    /** 非空时覆盖默认上报 URL。 */
    private String telemetryUrl;

    /**
     * 获取代理 URL。
     *
     * @return 代理 URL，可能为 null
     */
    public String getProxy() {
        return proxy;
    }

    /**
     * 设置代理 URL。
     *
     * @param proxy 代理 URL
     * @return 当前对象，便于链式调用
     */
    public SdkConfig setProxy(String proxy) {
        this.proxy = proxy;
        return this;
    }

    /**
     * 获取超时秒数。
     *
     * @return 超时秒数
     */
    public int getTimeout() {
        return timeout;
    }

    /**
     * 设置超时秒数。
     *
     * @param timeout 超时秒数
     * @return 当前对象，便于链式调用
     */
    public SdkConfig setTimeout(int timeout) {
        this.timeout = timeout;
        return this;
    }

    /**
     * 是否跳过 TLS 验证。
     *
     * @return 跳过返回 true
     */
    public boolean isInsecure() {
        return insecure;
    }

    /**
     * 设置是否跳过 TLS 验证。
     *
     * @param insecure 跳过返回 true
     * @return 当前对象，便于链式调用
     */
    public SdkConfig setInsecure(boolean insecure) {
        this.insecure = insecure;
        return this;
    }

    /**
     * 获取自定义请求头。
     *
     * @return 请求头映射，可能为 null
     */
    public Map<String, String> getHeaders() {
        return headers;
    }

    /**
     * 设置自定义请求头。
     *
     * @param headers 请求头映射
     * @return 当前对象，便于链式调用
     */
    public SdkConfig setHeaders(Map<String, String> headers) {
        this.headers = headers;
        return this;
    }

    /**
     * 获取 DropMail 令牌。
     *
     * @return 令牌，可能为 null
     */
    public String getDropmailAuthToken() {
        return dropmailAuthToken;
    }

    /**
     * 设置 DropMail 令牌。
     *
     * @param dropmailAuthToken 令牌
     * @return 当前对象，便于链式调用
     */
    public SdkConfig setDropmailAuthToken(String dropmailAuthToken) {
        this.dropmailAuthToken = dropmailAuthToken;
        return this;
    }

    /**
     * 获取 apihz id。
     *
     * @return apihz id，可能为 null
     */
    public String getApihzId() {
        return apihzId;
    }

    /**
     * 设置 apihz id。
     *
     * @param apihzId apihz id
     * @return 当前对象，便于链式调用
     */
    public SdkConfig setApihzId(String apihzId) {
        this.apihzId = apihzId;
        return this;
    }

    /**
     * 获取 apihz key。
     *
     * @return apihz key，可能为 null
     */
    public String getApihzKey() {
        return apihzKey;
    }

    /**
     * 设置 apihz key。
     *
     * @param apihzKey apihz key
     * @return 当前对象，便于链式调用
     */
    public SdkConfig setApihzKey(String apihzKey) {
        this.apihzKey = apihzKey;
        return this;
    }

    /**
     * 获取遥测开关。
     *
     * @return null 表示默认开启，可能为 null
     */
    public Boolean getTelemetryEnabled() {
        return telemetryEnabled;
    }

    /**
     * 设置遥测开关。
     *
     * @param telemetryEnabled 开关，null 表示默认开启
     * @return 当前对象，便于链式调用
     */
    public SdkConfig setTelemetryEnabled(Boolean telemetryEnabled) {
        this.telemetryEnabled = telemetryEnabled;
        return this;
    }

    /**
     * 获取自定义遥测 URL。
     *
     * @return 遥测 URL，可能为 null
     */
    public String getTelemetryUrl() {
        return telemetryUrl;
    }

    /**
     * 设置自定义遥测 URL。
     *
     * @param telemetryUrl 遥测 URL
     * @return 当前对象，便于链式调用
     */
    public SdkConfig setTelemetryUrl(String telemetryUrl) {
        this.telemetryUrl = telemetryUrl;
        return this;
    }
}
