<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * uncorreotemporal.com 渠道实现
 *
 * API: https://uncorreotemporal.com/api/v1
 * 流程: POST /mailboxes 创建邮箱（address + session_token）→
 *       GET /mailboxes/{email}/messages?limit=50 列表 → GET /mailboxes/{email}/messages/{id} 详情。
 * 认证: 请求头 X-Session-Token。
 */
final class UnCorreoTemporal
{
    private const CHANNEL = 'uncorreotemporal';
    private const API_BASE = 'https://uncorreotemporal.com/api/v1';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'Origin' => 'https://uncorreotemporal.com',
        'Referer' => 'https://uncorreotemporal.com/',
        'User-Agent' => 'Mozilla/5.0',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(
            self::API_BASE . '/mailboxes',
            self::HEADERS + ['Content-Type' => 'application/json'],
        );
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['address'] ?? ''));
        $token = trim((string) ($data['session_token'] ?? ''));
        if ($email === '' || !str_contains($email, '@') || $token === '') {
            throw new \RuntimeException('uncorreotemporal: 创建邮箱响应无效');
        }
        $exp = $data['expires_at'] ?? null;
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
            throw new \InvalidArgumentException('uncorreotemporal: session token 不能为空');
        }
        if ($address === '') {
            throw new \InvalidArgumentException('uncorreotemporal: 邮箱地址不能为空');
        }

        $resp = HttpClient::get(
            self::API_BASE . '/mailboxes/' . rawurlencode($address) . '/messages?limit=50',
            self::HEADERS + ['X-Session-Token' => $sessionToken],
        );
        $rows = HttpClient::json($resp);
        if (!array_is_list($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                continue;
            }
            $messageId = trim((string) ($row['id'] ?? ''));
            $detail = $messageId !== '' ? self::fetchDetail($address, $sessionToken, $messageId) : null;
            $out[] = Normalize::email(self::flatten($detail ?? $row, $address), $address);
        }
        return $out;
    }

    /**
     * @return array<string,mixed>|null
     */
    private static function fetchDetail(string $email, string $token, string $messageId): ?array
    {
        try {
            $resp = HttpClient::get(
                self::API_BASE . '/mailboxes/' . rawurlencode($email) . '/messages/' . rawurlencode($messageId),
                self::HEADERS + ['X-Session-Token' => $token],
            );
            if ($resp->getStatusCode() >= 400) {
                return null;
            }
            $data = HttpClient::json($resp);
            return !empty($data) ? $data : null;
        } catch (\Throwable) {
            return null;
        }
    }

    /**
     * @param array<mixed> $raw
     * @return array<string,mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        $attachments = $raw['attachments'] ?? null;
        return [
            'id' => $raw['id'] ?? '',
            'from' => $raw['from_address'] ?? '',
            'to' => $raw['to_address'] ?? $recipient,
            'subject' => $raw['subject'] ?? '',
            'text' => $raw['body_text'] ?? '',
            'html' => $raw['body_html'] ?? '',
            'date' => $raw['received_at'] ?? '',
            'isRead' => (bool) ($raw['is_read'] ?? false),
            'attachments' => is_array($attachments) ? $attachments : [],
        ];
    }
}
