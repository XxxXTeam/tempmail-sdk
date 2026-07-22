package com.xxxxteam.tempmail;

/**
 * 邮件附件信息。
 */
public final class EmailAttachment {

    /** 附件文件名。 */
    private String filename = "";

    /** 附件大小（字节），可能为 null。 */
    private Long size;

    /** 附件 MIME 类型，可能为 null。 */
    private String contentType;

    /** 附件下载地址，可能为 null。 */
    private String url;

    /**
     * 默认构造。
     */
    public EmailAttachment() {
    }

    /**
     * 全字段构造。
     *
     * @param filename    文件名
     * @param size        大小（字节）
     * @param contentType MIME 类型
     * @param url         下载地址
     */
    public EmailAttachment(String filename, Long size, String contentType, String url) {
        this.filename = filename;
        this.size = size;
        this.contentType = contentType;
        this.url = url;
    }

    /**
     * 获取文件名。
     *
     * @return 文件名
     */
    public String getFilename() {
        return filename;
    }

    /**
     * 设置文件名。
     *
     * @param filename 文件名
     */
    public void setFilename(String filename) {
        this.filename = filename;
    }

    /**
     * 获取附件大小。
     *
     * @return 大小（字节），可能为 null
     */
    public Long getSize() {
        return size;
    }

    /**
     * 设置附件大小。
     *
     * @param size 大小（字节）
     */
    public void setSize(Long size) {
        this.size = size;
    }

    /**
     * 获取 MIME 类型。
     *
     * @return MIME 类型，可能为 null
     */
    public String getContentType() {
        return contentType;
    }

    /**
     * 设置 MIME 类型。
     *
     * @param contentType MIME 类型
     */
    public void setContentType(String contentType) {
        this.contentType = contentType;
    }

    /**
     * 获取下载地址。
     *
     * @return 下载地址，可能为 null
     */
    public String getUrl() {
        return url;
    }

    /**
     * 设置下载地址。
     *
     * @param url 下载地址
     */
    public void setUrl(String url) {
        this.url = url;
    }
}
