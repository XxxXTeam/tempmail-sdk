<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * shitty.email 渠道实现
 *
 * API: https://shitty.email/api
 * 流程: POST /inbox 创建收件箱（返回 email + token）→ GET /inbox 读列表 → GET /email/{id} 读详情。
 * 认证: 请求头 X-Session-Token。
 */
final class ShittyEmail
{
    private const CHANNEL = 'shitty-email';
    private const API_BASE = 'https://shitty.email/api';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0',
    ];

    /**
     * @return array<string,string>
     */
    private static function headers(?string $token = null): array
    {
        $h = self::HEADERS;
        if ($token !== null && $token !== '') {
            $h['X-Session-Token'] = $token;
        }
        return $h;
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::API_BASE . '/inbox', self::headers());
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['email'] ?? ''));
        $token = trim((string) ($data['token'] ?? ''));
        if ($email === '' || !str_contains($email, '@') || $token === '') {
            throw new \RuntimeException('shitty-email: 创建收件箱响应无效');
        }
        $exp = $data['expiresAt'] ?? null;
        return new EmailInfo(self::CHANNEL, $email, $token, expiresAt: is_numeric($exp) ? (int) $exp : null);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $sessionToken = trim((string) $token);
        $address = trim($email);
        if ($sessionToken === '') {
            throw new \InvalidArgumentException('shitty-email: token 不能为空');
        }
        if ($address === '') {
            throw new \InvalidArgumentException('shitty-email: 邮箱地址不能为空');
        }

        $resp = HttpClient::get(self::API_BASE . '/inbox', self::headers($sessionToken));
        $data = HttpClient::json($resp);
        $rows = $data['emails'] ?? null;
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $messageId = trim((string) ($item['id'] ?? ''));
            $detail = $messageId !== '' ? self::fetchMessage($sessionToken, $messageId) : null;
            $out[] = Normalize::email(self::flatten($detail ?? $item, $address), $address);
        }
        return $out;
    }

    /**
     * @return array<string,mixed>|null
     */
    private static function fetchMessage(string $token, string $messageId): ?array
    {
        $resp = HttpClient::get(self::API_BASE . '/email/' . rawurlencode($messageId), self::headers($token));
        if ($resp->getStatusCode() >= 400) {
            return null;
        }
        $data = HttpClient::json($resp);
        return !empty($data) ? $data : null;
    }

    /**
     * @param array<mixed> $raw
     * @return array<string,mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        return [
            'id' => $raw['id'] ?? '',
            'from' => $raw['from'] ?? '',
            'to' => $raw['to'] ?? $recipient,
            'subject' => $raw['subject'] ?? '',
            'text' => $raw['text'] ?? '',
            'html' => $raw['html'] ?? '',
            'date' => $raw['date'] ?? '',
        ];
    }
}
