<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 重试配置
 */
final class RetryConfig
{
    /**
     * @param int   $maxRetries   最大重试次数（不含首次请求），默认 2
     * @param float $initialDelay 初始重试延迟（秒），默认 1.0
     * @param float $maxDelay     最大重试延迟（秒），默认 5.0
     * @param float $timeout      请求超时（秒），默认 15.0
     */
    public function __construct(
        public int $maxRetries = 2,
        public float $initialDelay = 1.0,
        public float $maxDelay = 5.0,
        public float $timeout = 15.0,
    ) {
    }
}
