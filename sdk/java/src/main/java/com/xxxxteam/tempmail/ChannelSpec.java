package com.xxxxteam.tempmail;

import java.util.List;

/**
 * 单个渠道的注册规格（注册表范式的核心）。
 *
 * <p>每新增一个渠道只需追加一处 {@code Registry.register(...)}，
 * 渠道列表、信息映射、创建/收信分发全部由该结构自动派生。</p>
 */
public final class ChannelSpec {

    /**
     * 创建邮箱的函数式接口。
     */
    @FunctionalInterface
    public interface GenerateFn {
        /**
         * 按选项创建邮箱。
         *
         * @param options 创建选项
         * @return 邮箱信息
         * @throws Exception 创建失败
         */
        EmailInfo generate(GenerateEmailOptions options) throws Exception;
    }

    /**
     * 获取邮件的函数式接口。
     */
    @FunctionalInterface
    public interface GetEmailsFn {
        /**
         * 按邮箱地址与 token 获取邮件。
         *
         * @param email 邮箱地址
         * @param token 认证令牌，可为 null
         * @return 邮件列表
         * @throws Exception 获取失败
         */
        List<Email> getEmails(String email, String token) throws Exception;
    }

    /** 渠道标识。 */
    private final String channel;

    /** 渠道显示名称。 */
    private final String name;

    /** 对应的临时邮箱服务网站。 */
    private final String website;

    /** 创建邮箱的实现。 */
    private final GenerateFn generate;

    /** 获取邮件的实现。 */
    private final GetEmailsFn getEmails;

    /**
     * 构造渠道规格。
     *
     * @param channel   渠道标识
     * @param name      显示名称
     * @param website   服务网站
     * @param generate  创建实现
     * @param getEmails 收信实现
     */
    public ChannelSpec(String channel, String name, String website,
                       GenerateFn generate, GetEmailsFn getEmails) {
        this.channel = channel;
        this.name = name;
        this.website = website;
        this.generate = generate;
        this.getEmails = getEmails;
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

    /**
     * 获取创建实现。
     *
     * @return 创建函数
     */
    public GenerateFn getGenerate() {
        return generate;
    }

    /**
     * 获取收信实现。
     *
     * @return 收信函数
     */
    public GetEmailsFn getGetEmails() {
        return getEmails;
    }
}
