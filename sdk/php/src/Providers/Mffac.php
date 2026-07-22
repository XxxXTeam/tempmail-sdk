<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * MFFAC 渠道实现 — https://www.mffac.com/api
 *
 * POST /mailboxes 创建邮箱（地址为本地部分，需补 @mffac.com）；
 * GET /mailboxes/{local}/emails 列表；GET /emails/{id} 详情。
 */
final class Mffac
{
    private const CHANNEL = 'mffac';
    private const BASE = 'https://www.mffac.com/api';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
        'Content-Type' => 'application/json',
        'Accept' => '*/*',
        'Origin' => 'https://www.mffac.com',
        'Referer' => 'https://www.mffac.com/',
    ];

    /**
     * 构造不含 Content-Type 的 GET 请求头。
     *
     * @return array<string,string>
     */
    private static function getHeaders(): array
    {
        $h = self::HEADERS;
        unset($h['Content-Type']);
        return $h;
    }

    /**
     * 将秒级时间戳转为 ISO 8601（UTC）；无效返回空串。
     */
    private static function receivedAtToIso(mixed $value): string
    {
        if (!is_numeric($value)) {
            return '';
        }
        $seconds = (float) $value;
        if ($seconds <= 0) {
            return '';
        }
        return gmdate('Y-m-d\TH:i:s\Z', (int) $seconds);
    }

    /**
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        return [
            'id' => $raw['id'] ?? '',
            'from' => $raw['fromAddress'] ?? '',
            'to' => $raw['toAddress'] ?? $recipient,
            'subject' => $raw['subject'] ?? '',
            'text' => $raw['textContent'] ?? '',
            'html' => $raw['htmlContent'] ?? '',
            'date' => self::receivedAtToIso($raw['receivedAt'] ?? null),
            'isRead' => $raw['isRead'] ?? false,
            'attachments' => [],
        ];
    }

    /**
     * 拉取单封邮件详情，失败返回 null。
     *
     * @return array<mixed>|null
     */
    private static function fetchDetail(string $messageId): ?array
    {
        $resp = HttpClient::get(self::BASE . '/emails/' . rawurlencode($messageId), self::getHeaders());
        if ($resp->getStatusCode() >= 400) {
            return null;
        }
        $data = HttpClient::json($resp);
        $email = (!empty($data['success']) ? ($data['email'] ?? null) : null);
        return is_array($email) ? $email : null;
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::BASE . '/mailboxes', self::HEADERS, json: ['expiresInHours' => 24]);
        $data = HttpClient::json($resp);
        $mb = $data['mailbox'] ?? null;
        if (empty($data['success']) || !is_array($mb)) {
            throw new \RuntimeException('mffac: 创建邮箱失败');
        }
        $addr = trim((string) ($mb['address'] ?? ''));
        $mid = trim((string) ($mb['id'] ?? ''));
        if ($addr === '' || $mid === '') {
            throw new \RuntimeException('mffac: 无效的邮箱');
        }
        return new EmailInfo(self::CHANNEL, $addr . '@mffac.com', $mid, createdAt: $mb['createdAt'] ?? null);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $local = $email !== '' ? trim(explode('@', $email, 2)[0]) : '';
        if ($local === '') {
            throw new \InvalidArgumentException('mffac: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::BASE . '/mailboxes/' . rawurlencode($local) . '/emails', self::getHeaders());
        $data = HttpClient::json($resp);
        if (empty($data['success'])) {
            throw new \RuntimeException('mffac: 获取列表失败');
        }
        $rows = $data['emails'] ?? [];
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $messageId = trim((string) ($raw['id'] ?? ''));
            $detail = $messageId !== '' ? self::fetchDetail($messageId) : null;
            $out[] = Normalize::email(self::flatten($detail ?? $raw, $email), $email);
        }
        return $out;
    }
}
