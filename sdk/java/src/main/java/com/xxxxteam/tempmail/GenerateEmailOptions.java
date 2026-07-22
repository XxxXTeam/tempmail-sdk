package com.xxxxteam.tempmail;

import java.util.List;

/**
 * 创建临时邮箱的选项。
 */
public final class GenerateEmailOptions {

    /** 指定渠道，不指定则随机选择。 */
    private String channel;

    /** tempmail 渠道的有效期（分钟）。 */
    private int duration = 30;

    /** 指定域名（tempmail-cn / tempmail-lol / maildrop / fake-legal / mail-cx 等）。 */
    private String domain;

    /** 重试配置。 */
    private RetryConfig retry;

    /** 最大尝试渠道数。 */
    private int maxChannelsTried = 20;

    /** 整体超时时间（毫秒）。 */
    private long totalTimeoutMillis = 60000L;

    /** 邮箱后缀筛选（如 "@gmail.com" 或 "gmail.com"）。 */
    private String suffix;

    /** 多个目标域名（如 ["outlook.com", "hotmail.com"]）。 */
    private List<String> domains;

    /**
     * 获取指定渠道。
     *
     * @return 渠道标识，可能为 null
     */
    public String getChannel() {
        return channel;
    }

    /**
     * 设置指定渠道。
     *
     * @param channel 渠道标识
     * @return 当前对象，便于链式调用
     */
    public GenerateEmailOptions setChannel(String channel) {
        this.channel = channel;
        return this;
    }

    /**
     * 获取有效期（分钟）。
     *
     * @return 有效期
     */
    public int getDuration() {
        return duration;
    }

    /**
     * 设置有效期（分钟）。
     *
     * @param duration 有效期
     * @return 当前对象，便于链式调用
     */
    public GenerateEmailOptions setDuration(int duration) {
        this.duration = duration;
        return this;
    }

    /**
     * 获取指定域名。
     *
     * @return 域名，可能为 null
     */
    public String getDomain() {
        return domain;
    }

    /**
     * 设置指定域名。
     *
     * @param domain 域名
     * @return 当前对象，便于链式调用
     */
    public GenerateEmailOptions setDomain(String domain) {
        this.domain = domain;
        return this;
    }

    /**
     * 获取重试配置。
     *
     * @return 重试配置，可能为 null
     */
    public RetryConfig getRetry() {
        return retry;
    }

    /**
     * 设置重试配置。
     *
     * @param retry 重试配置
     * @return 当前对象，便于链式调用
     */
    public GenerateEmailOptions setRetry(RetryConfig retry) {
        this.retry = retry;
        return this;
    }

    /**
     * 获取最大尝试渠道数。
     *
     * @return 最大尝试渠道数
     */
    public int getMaxChannelsTried() {
        return maxChannelsTried;
    }

    /**
     * 设置最大尝试渠道数。
     *
     * @param maxChannelsTried 最大尝试渠道数
     * @return 当前对象，便于链式调用
     */
    public GenerateEmailOptions setMaxChannelsTried(int maxChannelsTried) {
        this.maxChannelsTried = maxChannelsTried;
        return this;
    }

    /**
     * 获取整体超时时间。
     *
     * @return 超时（毫秒）
     */
    public long getTotalTimeoutMillis() {
        return totalTimeoutMillis;
    }

    /**
     * 设置整体超时时间。
     *
     * @param totalTimeoutMillis 超时（毫秒）
     * @return 当前对象，便于链式调用
     */
    public GenerateEmailOptions setTotalTimeoutMillis(long totalTimeoutMillis) {
        this.totalTimeoutMillis = totalTimeoutMillis;
        return this;
    }

    /**
     * 获取后缀筛选。
     *
     * @return 后缀，可能为 null
     */
    public String getSuffix() {
        return suffix;
    }

    /**
     * 设置后缀筛选。
     *
     * @param suffix 后缀
     * @return 当前对象，便于链式调用
     */
    public GenerateEmailOptions setSuffix(String suffix) {
        this.suffix = suffix;
        return this;
    }

    /**
     * 获取目标域名列表。
     *
     * @return 域名列表，可能为 null
     */
    public List<String> getDomains() {
        return domains;
    }

    /**
     * 设置目标域名列表。
     *
     * @param domains 域名列表
     * @return 当前对象，便于链式调用
     */
    public GenerateEmailOptions setDomains(List<String> domains) {
        this.domains = domains;
        return this;
    }
}
