<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 邮件附件信息
 *
 * 各渠道附件字段异构，统一映射到本结构。
 */
final class EmailAttachment
{
    /**
     * @param string      $filename    附件文件名
     * @param int|null    $size        附件大小（字节）
     * @param string|null $contentType 附件 MIME 类型
     * @param string|null $url         附件下载地址
     */
    public function __construct(
        public string $filename = '',
        public ?int $size = null,
        public ?string $contentType = null,
        public ?string $url = null,
    ) {
    }
}
