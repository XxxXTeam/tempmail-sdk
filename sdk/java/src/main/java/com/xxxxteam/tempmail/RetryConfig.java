package com.xxxxteam.tempmail;

/**
 * 重试配置。
 */
public final class RetryConfig {

    /** 最大重试次数（不含首次请求），默认 2。 */
    private int maxRetries = 2;

    /** 初始重试延迟（毫秒），默认 1000。 */
    private long initialDelayMillis = 1000L;

    /** 最大重试延迟（毫秒），默认 5000。 */
    private long maxDelayMillis = 5000L;

    /** 请求超时（毫秒），默认 15000。 */
    private long timeoutMillis = 15000L;

    /**
     * 获取最大重试次数。
     *
     * @return 最大重试次数
     */
    public int getMaxRetries() {
        return maxRetries;
    }

    /**
     * 设置最大重试次数。
     *
     * @param maxRetries 最大重试次数
     * @return 当前对象，便于链式调用
     */
    public RetryConfig setMaxRetries(int maxRetries) {
        this.maxRetries = maxRetries;
        return this;
    }

    /**
     * 获取初始重试延迟。
     *
     * @return 初始延迟（毫秒）
     */
    public long getInitialDelayMillis() {
        return initialDelayMillis;
    }

    /**
     * 设置初始重试延迟。
     *
     * @param initialDelayMillis 初始延迟（毫秒）
     * @return 当前对象，便于链式调用
     */
    public RetryConfig setInitialDelayMillis(long initialDelayMillis) {
        this.initialDelayMillis = initialDelayMillis;
        return this;
    }

    /**
     * 获取最大重试延迟。
     *
     * @return 最大延迟（毫秒）
     */
    public long getMaxDelayMillis() {
        return maxDelayMillis;
    }

    /**
     * 设置最大重试延迟。
     *
     * @param maxDelayMillis 最大延迟（毫秒）
     * @return 当前对象，便于链式调用
     */
    public RetryConfig setMaxDelayMillis(long maxDelayMillis) {
        this.maxDelayMillis = maxDelayMillis;
        return this;
    }

    /**
     * 获取请求超时。
     *
     * @return 超时（毫秒）
     */
    public long getTimeoutMillis() {
        return timeoutMillis;
    }

    /**
     * 设置请求超时。
     *
     * @param timeoutMillis 超时（毫秒）
     * @return 当前对象，便于链式调用
     */
    public RetryConfig setTimeoutMillis(long timeoutMillis) {
        this.timeoutMillis = timeoutMillis;
        return this;
    }
}
