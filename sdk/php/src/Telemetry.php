<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

use GuzzleHttp\Client;

/**
 * 匿名遥测上报
 *
 * 默认开启，可通过环境变量 TEMPMAIL_TELEMETRY_ENABLED=false 或配置关闭。
 * 上报内容仅为操作类型、渠道、成功与否等匿名统计信息，错误消息中的邮箱地址会被脱敏。
 */
final class Telemetry
{
    private const DEFAULT_URL = 'https://sdk-1.openel.top/v1/event';
    private const SDK_VERSION = '0.1.0';

    /** @var Client|null 复用的上报专用 Guzzle 客户端（避免每次上报重建 handler 栈） */
    private static ?Client $client = null;

    /**
     * 获取复用的上报客户端（首次调用时创建并缓存）。
     *
     * 遥测为高频热路径（每次生成邮箱/收信均触发），复用同一客户端可省去
     * 重复构建 handler 栈与中间件的开销，并允许到上报端点的连接 keep-alive 复用。
     */
    private static function client(): Client
    {
        if (self::$client === null) {
            self::$client = new Client(['timeout' => 8]);
        }
        return self::$client;
    }

    /**
     * 判断遥测是否开启。
     */
    private static function enabled(): bool
    {
        $v = ConfigStore::get()->telemetryEnabled;
        return $v === null ? true : $v;
    }

    private static function resolveUrl(): string
    {
        $u = trim((string) (ConfigStore::get()->telemetryUrl ?? ''));
        return $u !== '' ? $u : self::DEFAULT_URL;
    }

    /**
     * 脱敏错误消息中的邮箱地址，并截断长度。
     */
    private static function sanitize(string $msg): string
    {
        if ($msg === '') {
            return '';
        }
        $redacted = preg_replace('/[^\s@]{1,64}@[^\s@]{1,255}/', '[redacted]', $msg) ?? $msg;
        return substr($redacted, 0, 400);
    }

    /**
     * 上报一次操作事件（best-effort，异常静默）。
     */
    public static function report(
        string $operation,
        string $channel,
        bool $success,
        int $attemptCount,
        int $channelsTried,
        string $error,
    ): void {
        if (!self::enabled()) {
            return;
        }

        $ev = [
            'operation' => $operation,
            'channel' => $channel,
            'success' => $success,
            'attempt_count' => $attemptCount,
            'ts_ms' => (int) (microtime(true) * 1000),
        ];
        if ($channelsTried > 0) {
            $ev['channels_tried'] = $channelsTried;
        }
        $err = self::sanitize($error);
        if ($err !== '') {
            $ev['error'] = $err;
        }

        $batch = [
            'schema_version' => 2,
            'sdk_language' => 'php',
            'sdk_version' => self::SDK_VERSION,
            'os' => strtolower(PHP_OS),
            'arch' => php_uname('m'),
            'events' => [$ev],
        ];

        try {
            $body = json_encode($batch, JSON_UNESCAPED_UNICODE);
            self::client()->post(self::resolveUrl(), [
                'body' => $body,
                'headers' => [
                    'Content-Type' => 'application/json; charset=utf-8',
                    'User-Agent' => 'tempmail-sdk-php/' . self::SDK_VERSION,
                ],
                'http_errors' => false,
            ]);
        } catch (\Throwable) {
            // 遥测失败不影响主流程
        }
    }
}
