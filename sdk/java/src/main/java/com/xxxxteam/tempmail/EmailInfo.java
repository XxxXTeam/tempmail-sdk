package com.xxxxteam.tempmail;

/**
 * 创建临时邮箱后返回的邮箱信息。
 *
 * <p>Token 等认证信息由 SDK 内部维护，通过包级可见的访问器暴露，不对最终用户开放。</p>
 */
public final class EmailInfo {

    /** 创建该邮箱所使用的渠道标识。 */
    private final String channel;

    /** 临时邮箱地址。 */
    private final String email;

    /** 认证令牌，SDK 内部维护，不对外暴露。 */
    private final String token;

    /** 邮箱过期时间（毫秒时间戳），可能为 null。 */
    private final Long expiresAt;

    /** 邮箱创建时间（渠道原样返回的字符串），可能为 null。 */
    private final String createdAt;

    /**
     * 构造邮箱信息。
     *
     * @param channel   渠道标识
     * @param email     邮箱地址
     * @param token     内部令牌，可为 null
     * @param expiresAt 过期毫秒时间戳，可为 null
     * @param createdAt 创建时间字符串，可为 null
     */
    public EmailInfo(String channel, String email, String token, Long expiresAt, String createdAt) {
        this.channel = channel;
        this.email = email;
        this.token = token;
        this.expiresAt = expiresAt;
        this.createdAt = createdAt;
    }

    /**
     * 仅指定渠道与邮箱的便捷构造。
     *
     * @param channel 渠道标识
     * @param email   邮箱地址
     */
    public EmailInfo(String channel, String email) {
        this(channel, email, null, null, null);
    }

    /**
     * 获取渠道标识。
     *
     * @return 渠道标识
     */
    public String getChannel() {
        return channel;
    }

    /**
     * 获取邮箱地址。
     *
     * @return 邮箱地址
     */
    public String getEmail() {
        return email;
    }

    /**
     * 获取过期毫秒时间戳。
     *
     * @return 过期时间，可能为 null
     */
    public Long getExpiresAt() {
        return expiresAt;
    }

    /**
     * 获取创建时间字符串。
     *
     * @return 创建时间，可能为 null
     */
    public String getCreatedAt() {
        return createdAt;
    }

    /**
     * 获取内部令牌。仅供 SDK 内部分发使用。
     *
     * @return 令牌，可能为 null
     */
    String getToken() {
        return token;
    }

    /**
     * 返回带新渠道标识的副本，用于同一 provider 派生多个渠道别名。
     *
     * @param newChannel 覆写后的渠道标识
     * @return 新的 EmailInfo 实例
     */
    public EmailInfo withChannel(String newChannel) {
        return new EmailInfo(newChannel, email, token, expiresAt, createdAt);
    }

    @Override
    public String toString() {
        return "EmailInfo{channel='" + channel + "', email='" + email
                + "', expiresAt=" + expiresAt + ", createdAt='" + createdAt + "'}";
    }
}
