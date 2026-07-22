<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * OpenInbox 渠道实现 — https://openinbox.io
 *
 * POST /api/inbox 创建收件箱（token 存 inbox id）；
 * GET /api/emails/inbox/{id} 拉列表，GET /api/emails/{id} 拉单封详情。
 */
final class Openinbox
{
    private const API_BASE = 'https://api.openinbox.io/api';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json, text/plain, */*',
        'Origin' => 'https://openinbox.io',
        'Referer' => 'https://openinbox.io/',
        'User-Agent' => 'Mozilla/5.0',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(
            self::API_BASE . '/inbox',
            self::HEADERS + ['Content-Type' => 'application/json'],
        );
        $data = HttpClient::json($resp);
        $inboxId = trim((string) ($data['id'] ?? ''));
        $email = trim((string) ($data['email'] ?? ''));
        if ($inboxId === '' || $email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('openinbox: 无效的邮箱响应');
        }
        return new EmailInfo('openinbox', $email, $inboxId);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $inboxId = trim((string) $token);
        $address = trim($email);
        if ($inboxId === '') {
            throw new \InvalidArgumentException('openinbox: inbox id 为空');
        }
        if ($address === '') {
            throw new \InvalidArgumentException('openinbox: 邮箱地址为空');
        }

        $resp = HttpClient::get(
            self::API_BASE . '/emails/inbox/' . rawurlencode($inboxId) . '?page=1&limit=50',
            self::HEADERS,
        );
        $data = HttpClient::json($resp);
        $rows = $data['emails'] ?? [];
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                continue;
            }
            $messageId = trim((string) ($row['id'] ?? ''));
            $detail = $messageId !== '' ? self::fetchDetail($messageId) : null;
            $out[] = Normalize::email(self::flatten($detail ?? $row, $address), $address);
        }
        return $out;
    }

    /**
     * 拉取单封邮件详情；失败返回 null。
     *
     * @return array<mixed>|null
     */
    private static function fetchDetail(string $messageId): ?array
    {
        try {
            $resp = HttpClient::get(self::API_BASE . '/emails/' . rawurlencode($messageId), self::HEADERS);
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
        return [
            'id' => $raw['id'] ?? '',
            'from' => $raw['from'] ?? '',
            'to' => $recipient,
            'subject' => $raw['subject'] ?? '',
            'text' => $raw['textBody'] ?? '',
            'html' => $raw['htmlBody'] ?? '',
            'receivedAt' => $raw['receivedAt'] ?? '',
            'isRead' => (bool) ($raw['isRead'] ?? false),
        ];
    }
}
