<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 获取邮件列表的结果
 */
final class GetEmailsResult
{
    /**
     * @param string  $channel 渠道标识
     * @param string  $email   邮箱地址
     * @param Email[] $emails  邮件列表
     * @param bool    $success 本次请求是否成功
     */
    public function __construct(
        public string $channel = '',
        public string $email = '',
        public array $emails = [],
        public bool $success = true,
    ) {
    }
}
