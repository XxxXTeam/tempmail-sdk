<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * tempmail.ing 渠道实现
 *
 * API: https://api.tempmail.ing/api
 * POST /generate 创建（支持有效期分钟数）；GET /emails/{email} 收信。
 */
final class Tempmail
{
    private const CHANNEL = 'tempmail';
    private const BASE_URL = 'https://api.tempmail.ing/api';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36',
        'Content-Type' => 'application/json',
        'Referer' => 'https://tempmail.ing/',
        'DNT' => '1',
    ];

    /**
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        $isRead = ($raw['is_read'] ?? null) === 1 || ($raw['is_read'] ?? null) === true;
        return [
            'id' => $raw['id'] ?? '',
            'from' => $raw['from_address'] ?? $raw['from'] ?? '',
            'to' => $recipient,
            'subject' => $raw['subject'] ?? '',
            'text' => $raw['text'] ?? '',
            'html' => $raw['content'] ?? $raw['html'] ?? '',
            'date' => $raw['received_at'] ?? $raw['date'] ?? '',
            'is_read' => $isRead,
            'attachments' => $raw['attachments'] ?? [],
        ];
    }

    public static function generate(int $duration = 30): EmailInfo
    {
        $resp = HttpClient::post(self::BASE_URL . '/generate', self::HEADERS, json: ['duration' => $duration]);
        $data = HttpClient::json($resp);
        if (empty($data['success']) || !is_array($data['email'] ?? null)) {
            throw new \RuntimeException('tempmail: 创建邮箱失败');
        }
        $email = $data['email'];
        $expiresAt = $email['expiresAt'] ?? null;
        return new EmailInfo(
            self::CHANNEL,
            (string) $email['address'],
            expiresAt: is_numeric($expiresAt) ? (int) $expiresAt : null,
            createdAt: $email['createdAt'] ?? null,
        );
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('tempmail: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::BASE_URL . '/emails/' . rawurlencode($addr), self::HEADERS);
        $data = HttpClient::json($resp);
        if (empty($data['success'])) {
            throw new \RuntimeException('tempmail: 获取邮件失败');
        }
        $rows = $data['emails'] ?? [];
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $raw) {
            if (is_array($raw)) {
                $out[] = Normalize::email(self::flatten($raw, $addr), $addr);
            }
        }
        return $out;
    }
}
