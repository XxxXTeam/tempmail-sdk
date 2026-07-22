<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 创建临时邮箱的选项
 */
final class GenerateEmailOptions
{
    /**
     * @param string|null   $channel         指定渠道，不指定则随机选择
     * @param int           $duration        tempmail 渠道的有效期（分钟）
     * @param string|null   $domain          指定域名（tempmail-cn / tempmail-lol / maildrop / fake-legal / mail-cx 等）
     * @param RetryConfig|null $retry        重试配置
     * @param int           $maxChannelsTried 最大尝试渠道数
     * @param float         $totalTimeout    整体超时时间（秒）
     * @param string|null   $suffix          邮箱后缀筛选（如 "@gmail.com" 或 "gmail.com"）
     * @param string[]|null $domains         多个目标域名
     */
    public function __construct(
        public ?string $channel = null,
        public int $duration = 30,
        public ?string $domain = null,
        public ?RetryConfig $retry = null,
        public int $maxChannelsTried = 20,
        public float $totalTimeout = 60.0,
        public ?string $suffix = null,
        public ?array $domains = null,
    ) {
    }
}
