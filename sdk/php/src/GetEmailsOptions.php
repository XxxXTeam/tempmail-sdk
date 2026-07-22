<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 获取邮件的选项
 *
 * Channel/Email/Token 等由 SDK 从 EmailInfo 中自动获取，用户无需手动传递。
 */
final class GetEmailsOptions
{
    /**
     * @param RetryConfig|null $retry 重试配置
     */
    public function __construct(
        public ?RetryConfig $retry = null,
    ) {
    }
}
