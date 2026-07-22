<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 通用重试工具
 *
 * 提供请求重试、指数退避等错误恢复机制。网络错误、429、HTTP 4xx/5xx 会触发重试；
 * SDK 内部参数校验类错误不重试。
 */
final class Retry
{
    /**
     * 判断错误是否应该重试。
     */
    private static function shouldRetry(\Throwable $error): bool
    {
        $msg = strtolower($error->getMessage());

        $networkKeywords = [
            'connection', 'timeout', 'timed out', 'dns', 'eof',
            'broken pipe', 'network is unreachable', 'refused', 'reset',
            'curl error',
        ];
        foreach ($networkKeywords as $kw) {
            if (str_contains($msg, $kw)) {
                return true;
            }
        }

        if (str_contains($msg, '429') || str_contains($msg, 'too many requests') || str_contains($msg, 'rate limit')) {
            return true;
        }

        if (str_contains($msg, ': 4') || str_contains($msg, ': 5')) {
            return true;
        }

        return false;
    }

    /**
     * 执行带重试的操作，返回结果对象（失败不抛异常）。
     *
     * @param callable():mixed $fn
     */
    public static function withAttempts(callable $fn, ?RetryConfig $config = null): RetryResult
    {
        $cfg = $config ?? new RetryConfig();
        $lastError = null;

        for ($attempt = 0; $attempt <= $cfg->maxRetries; $attempt++) {
            $attempts = $attempt + 1;
            try {
                $result = $fn();
                if ($attempt > 0) {
                    Logger::info('第 ' . ($attempt + 1) . ' 次尝试成功');
                }
                return new RetryResult(ok: true, attempts: $attempts, value: $result);
            } catch (\Throwable $e) {
                $lastError = $e;
                if ($attempt >= $cfg->maxRetries || !self::shouldRetry($e)) {
                    return new RetryResult(ok: false, attempts: $attempts, error: $e);
                }
                $delay = min($cfg->initialDelay * (2 ** $attempt), $cfg->maxDelay);
                Logger::warning('请求失败（' . $e->getMessage() . '），' . $delay . 's 后重试...');
                usleep((int) ($delay * 1_000_000));
            }
        }

        return new RetryResult(ok: false, attempts: $cfg->maxRetries + 1, error: $lastError);
    }
}
