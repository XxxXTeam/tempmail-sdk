<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Tempy Email 渠道实现 — https://tempy.email
 *
 * POST /api/v1/mailbox 创建邮箱；GET /api/v1/mailbox/{email}/messages 收信。
 */
final class TempyEmail
{
    private const CHANNEL = 'tempy-email';
    private const API_BASE = 'https://tempy.email/api/v1';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
    ];

    /**
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        return [
            'id' => $raw['id'] ?? $raw['messageId'] ?? $raw['message_id'] ?? $raw['mail_id'] ?? '',
            'from' => $raw['from'] ?? $raw['sender'] ?? $raw['from_addr'] ?? $raw['from_address'] ?? '',
            'to' => $raw['to'] ?? $recipient,
            'subject' => $raw['subject'] ?? $raw['mail_title'] ?? '',
            'text' => $raw['text'] ?? $raw['body_text'] ?? $raw['text_body'] ?? $raw['body'] ?? '',
            'html' => $raw['html'] ?? $raw['body_html'] ?? $raw['html_body'] ?? '',
            'date' => $raw['date'] ?? $raw['received_at'] ?? $raw['created_at'] ?? '',
            'is_read' => $raw['is_read'] ?? $raw['isRead'] ?? $raw['seen'] ?? false,
            'attachments' => $raw['attachments'] ?? [],
        ];
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        $body = [];
        $wanted = trim((string) ($domain ?? ''));
        if ($wanted !== '') {
            $body['domain'] = $wanted;
        }
        $resp = HttpClient::post(
            self::API_BASE . '/mailbox',
            self::HEADERS + ['Content-Type' => 'application/json'],
            json: $body ?: new \stdClass(),
        );
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['email'] ?? ''));
        if ($email === '') {
            throw new \RuntimeException('tempy-email: 无效的创建响应');
        }
        return new EmailInfo(self::CHANNEL, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('tempy-email: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::API_BASE . '/mailbox/' . rawurlencode($addr) . '/messages', self::HEADERS);
        $data = HttpClient::json($resp);
        $rows = $data['messages'] ?? $data;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $item) {
            if (is_array($item)) {
                $out[] = Normalize::email(self::flatten($item, $addr), $addr);
            }
        }
        return $out;
    }
}
