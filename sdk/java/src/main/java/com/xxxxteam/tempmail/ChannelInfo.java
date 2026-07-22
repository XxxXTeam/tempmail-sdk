package com.xxxxteam.tempmail;

/**
 * 渠道信息。
 */
public final class ChannelInfo {

    /** 渠道标识。 */
    private final String channel;

    /** 渠道显示名称。 */
    private final String name;

    /** 对应的临时邮箱服务网站。 */
    private final String website;

    /**
     * 构造渠道信息。
     *
     * @param channel 渠道标识
     * @param name    显示名称
     * @param website 服务网站
     */
    public ChannelInfo(String channel, String name, String website) {
        this.channel = channel;
        this.name = name;
        this.website = website;
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
     * 获取显示名称。
     *
     * @return 显示名称
     */
    public String getName() {
        return name;
    }

    /**
     * 获取服务网站。
     *
     * @return 服务网站
     */
    public String getWebsite() {
        return website;
    }

    @Override
    public String toString() {
        return "ChannelInfo{channel='" + channel + "', name='" + name + "', website='" + website + "'}";
    }
}
