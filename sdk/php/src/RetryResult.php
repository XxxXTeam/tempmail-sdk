<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 重试执行结果：ok 为 true 时 value 有效，否则 error 有效。
 */
final class RetryResult
{
    public function __construct(
        public bool $ok,
        public int $attempts,
        public mixed $value = null,
        public ?\Throwable $error = null,
    ) {
    }
}
