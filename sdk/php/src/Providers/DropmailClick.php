<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * dropmail-click 渠道实现 — https://dropmail.click
 *
 * 免注册、无鉴权，Token 即邮箱地址本身。
 *   创建: POST /api/v1/public/mailbox → {address, expires_at}
 *   读信: GET  /api/v1/public/mailbox/{email} → {messages: [...]}
 */
final class DropmailClick
{
    private const CHANNEL = 'dropmail-click';
    private const BASE_URL = 'https://dropmail.click';

    private static function userAgent(): string
    {
        $headers = \ChanhanzhanX\TempMail\ConfigStore::get()->headers;
        foreach ($headers as $k => $v) {
            if (strtolower((string) $k) === 'user-agent' && $v !== '') {
                return (string) $v;
            }
        }
        return 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36';
    }

    /**
     * @return array<string,string>
     */
    private static function headers(): array
    {
        return ['User-Agent' => self::userAgent(), 'Accept' => 'application/json'];
    }

    /**
     * 按优先级提取首个非空值并转字符串。
     *
     * @param array<mixed> $msg
     */
    private static function first(array $msg, string ...$keys): string
    {
        foreach ($keys as $key) {
            $val = $msg[$key] ?? null;
            if ($val !== null && $val !== '') {
                return (string) $val;
            }
        }
        return '';
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::BASE_URL . '/api/v1/public/mailbox', self::headers());
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('dropmail-click: 创建邮箱失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $address = trim((string) ($data['address'] ?? ''));
        if ($address === '') {
            throw new \RuntimeException('dropmail-click: 无效响应，缺少 address');
        }
        $expiresAt = null;
        if (isset($data['expires_at']) && is_numeric($data['expires_at'])) {
            $expiresAt = (int) $data['expires_at'];
        }
        return new EmailInfo(self::CHANNEL, $address, $address, expiresAt: $expiresAt);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($token ?? '') !== '' ? trim((string) $token) : trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('dropmail-click: 缺少邮箱地址');
        }
        $resp = HttpClient::get(
            self::BASE_URL . '/api/v1/public/mailbox/' . rawurlencode($addr),
            self::headers(),
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('dropmail-click: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $messages = $data['messages'] ?? null;
        if (!is_array($messages)) {
            return [];
        }
        $out = [];
        foreach ($messages as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $out[] = Normalize::email([
                'id' => self::first($msg, 'id'),
                'from' => self::first($msg, 'from'),
                'to' => self::first($msg, 'address') ?: $addr,
                'subject' => self::first($msg, 'subject'),
                'text' => self::first($msg, 'text'),
                'html' => self::first($msg, 'html'),
                'date' => self::first($msg, 'received_at', 'date'),
            ], $addr);
        }
        return $out;
    }
}
