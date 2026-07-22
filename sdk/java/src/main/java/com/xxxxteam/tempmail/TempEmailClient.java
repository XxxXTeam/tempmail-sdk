package com.xxxxteam.tempmail;

/**
 * 临时邮箱客户端：封装邮箱创建与邮件获取的完整流程，自动管理 {@link EmailInfo} 与 token。
 */
public final class TempEmailClient {

    static {
        // 首次加载客户端类时按环境变量 TEMPMAIL_WEBUI 决定是否启动可视化面板（幂等）
        WebUI.maybeStartFromEnv();
    }

    /** 当前缓存的邮箱信息。 */
    private EmailInfo emailInfo;

    /**
     * 获取当前缓存的邮箱信息。
     *
     * @return 邮箱信息，未创建时为 null
     */
    public EmailInfo getEmailInfo() {
        return emailInfo;
    }

    /**
     * 使用默认选项创建临时邮箱并缓存邮箱信息。
     *
     * @return 邮箱信息
     */
    public EmailInfo generate() {
        return generate(null);
    }

    /**
     * 创建临时邮箱并缓存邮箱信息。
     *
     * @param options 创建选项，可为 null
     * @return 邮箱信息
     */
    public EmailInfo generate(GenerateEmailOptions options) {
        emailInfo = TempMail.generateEmail(options != null ? options : new GenerateEmailOptions());
        return emailInfo;
    }

    /**
     * 使用默认选项获取当前邮箱的邮件列表，必须先调用 {@link #generate()}。
     *
     * @return 收信结果
     */
    public GetEmailsResult getEmails() {
        return getEmails(null);
    }

    /**
     * 获取当前邮箱的邮件列表，必须先调用 {@link #generate()}。
     *
     * @param options 收信选项，可为 null
     * @return 收信结果
     */
    public GetEmailsResult getEmails(GetEmailsOptions options) {
        if (emailInfo == null) {
            Telemetry.report("get_emails", "", false, 0, 0, "No email generated. Call generate() first");
            throw new IllegalStateException("No email generated. Call generate() first");
        }
        return TempMail.getEmails(emailInfo, options);
    }
}
