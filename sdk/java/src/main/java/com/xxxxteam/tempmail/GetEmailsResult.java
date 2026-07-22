package com.xxxxteam.tempmail;

import java.util.ArrayList;
import java.util.List;

/**
 * 获取邮件列表的结果。
 */
public final class GetEmailsResult {

    /** 渠道标识。 */
    private final String channel;

    /** 邮箱地址。 */
    private final String email;

    /** 邮件列表。 */
    private final List<Email> emails;

    /** 本次请求是否成功。 */
    private final boolean success;

    /**
     * 构造结果。
     *
     * @param channel 渠道标识
     * @param email   邮箱地址
     * @param emails  邮件列表
     * @param success 是否成功
     */
    public GetEmailsResult(String channel, String email, List<Email> emails, boolean success) {
        this.channel = channel;
        this.email = email;
        this.emails = emails != null ? emails : new ArrayList<>();
        this.success = success;
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
     * 获取邮件列表。
     *
     * @return 邮件列表
     */
    public List<Email> getEmails() {
        return emails;
    }

    /**
     * 本次请求是否成功。
     *
     * @return 成功返回 true
     */
    public boolean isSuccess() {
        return success;
    }
}
