<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 标准化的邮件对象
 *
 * 所有渠道返回的原始数据都会经 Normalize 映射为本结构，保证跨渠道输出一致。
 */
final class Email
{
    /**
     * @param string               $id          邮件唯一标识
     * @param string               $fromAddr    发件人地址
     * @param string               $to          收件人地址
     * @param string               $subject     邮件主题
     * @param string               $text        纯文本内容
     * @param string               $html        HTML 内容
     * @param string               $date        接收日期（ISO 8601 格式）
     * @param bool                 $isRead      是否已读
     * @param EmailAttachment[]    $attachments 附件列表
     */
    public function __construct(
        public string $id = '',
        public string $fromAddr = '',
        public string $to = '',
        public string $subject = '',
        public string $text = '',
        public string $html = '',
        public string $date = '',
        public bool $isRead = false,
        public array $attachments = [],
    ) {
    }
}
