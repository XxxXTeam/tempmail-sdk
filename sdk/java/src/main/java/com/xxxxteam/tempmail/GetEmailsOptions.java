package com.xxxxteam.tempmail;

/**
 * 获取邮件的选项。
 *
 * <p>Channel/Email/Token 等由 SDK 从 {@link EmailInfo} 中自动获取，用户无需手动传递。</p>
 */
public final class GetEmailsOptions {

    /** 重试配置。 */
    private RetryConfig retry;

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
    public GetEmailsOptions setRetry(RetryConfig retry) {
        this.retry = retry;
        return this;
    }
}
