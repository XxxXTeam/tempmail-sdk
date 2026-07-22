package com.xxxxteam.tempmail;

import java.util.ArrayList;
import java.util.List;

/**
 * 标准化的邮件对象。
 *
 * <p>所有渠道返回的异构数据都会经 {@link Normalizer} 映射为该统一结构。</p>
 */
public final class Email {

    /** 邮件唯一标识。 */
    private String id = "";

    /** 发件人地址。 */
    private String from = "";

    /** 收件人地址。 */
    private String to = "";

    /** 邮件主题。 */
    private String subject = "";

    /** 纯文本内容。 */
    private String text = "";

    /** HTML 内容。 */
    private String html = "";

    /** 接收日期（ISO 8601 格式）。 */
    private String date = "";

    /** 是否已读。 */
    private boolean read = false;

    /** 附件列表。 */
    private List<EmailAttachment> attachments = new ArrayList<>();

    /**
     * 获取邮件 ID。
     *
     * @return 邮件 ID
     */
    public String getId() {
        return id;
    }

    /**
     * 设置邮件 ID。
     *
     * @param id 邮件 ID
     */
    public void setId(String id) {
        this.id = id;
    }

    /**
     * 获取发件人地址。
     *
     * @return 发件人地址
     */
    public String getFrom() {
        return from;
    }

    /**
     * 设置发件人地址。
     *
     * @param from 发件人地址
     */
    public void setFrom(String from) {
        this.from = from;
    }

    /**
     * 获取收件人地址。
     *
     * @return 收件人地址
     */
    public String getTo() {
        return to;
    }

    /**
     * 设置收件人地址。
     *
     * @param to 收件人地址
     */
    public void setTo(String to) {
        this.to = to;
    }

    /**
     * 获取邮件主题。
     *
     * @return 邮件主题
     */
    public String getSubject() {
        return subject;
    }

    /**
     * 设置邮件主题。
     *
     * @param subject 邮件主题
     */
    public void setSubject(String subject) {
        this.subject = subject;
    }

    /**
     * 获取纯文本内容。
     *
     * @return 纯文本内容
     */
    public String getText() {
        return text;
    }

    /**
     * 设置纯文本内容。
     *
     * @param text 纯文本内容
     */
    public void setText(String text) {
        this.text = text;
    }

    /**
     * 获取 HTML 内容。
     *
     * @return HTML 内容
     */
    public String getHtml() {
        return html;
    }

    /**
     * 设置 HTML 内容。
     *
     * @param html HTML 内容
     */
    public void setHtml(String html) {
        this.html = html;
    }

    /**
     * 获取接收日期。
     *
     * @return ISO 8601 日期字符串
     */
    public String getDate() {
        return date;
    }

    /**
     * 设置接收日期。
     *
     * @param date ISO 8601 日期字符串
     */
    public void setDate(String date) {
        this.date = date;
    }

    /**
     * 是否已读。
     *
     * @return 已读返回 true
     */
    public boolean isRead() {
        return read;
    }

    /**
     * 设置已读状态。
     *
     * @param read 已读状态
     */
    public void setRead(boolean read) {
        this.read = read;
    }

    /**
     * 获取附件列表。
     *
     * @return 附件列表
     */
    public List<EmailAttachment> getAttachments() {
        return attachments;
    }

    /**
     * 设置附件列表。
     *
     * @param attachments 附件列表
     */
    public void setAttachments(List<EmailAttachment> attachments) {
        this.attachments = attachments;
    }

    @Override
    public String toString() {
        return "Email{id='" + id + "', from='" + from + "', to='" + to
                + "', subject='" + subject + "', date='" + date + "', read=" + read
                + ", attachments=" + attachments.size() + "}";
    }
}
