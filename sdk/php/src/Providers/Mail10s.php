<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Mail10s 渠道实现 — https://mail10s.com
 *
 * 本地生成随机邮箱地址，读信 GET /api/emails/{address}/inbox，
 * 解析 data.messages 数组。Token 即邮箱地址本身。
 */
final class Mail10s
{
    private const CHANNEL = 'mail10s';
    private const BASE_URL = 'https://mail10s.com';
    private const DOMAIN = 'mail10s.com';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0',
    ];

    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = 'sdk';
        for ($i = 0; $i < 16; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(): EmailInfo
    {
        $email = self::randomLocal() . '@' . self::DOMAIN;
        return new EmailInfo(self::CHANNEL, $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('mail10s: empty email');
        }
        $resp = HttpClient::get(
            self::BASE_URL . '/api/emails/' . rawurlencode($address) . '/inbox',
            self::HEADERS,
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('mail10s: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $messages = $data['data']['messages'] ?? null;
        if (!is_array($messages)) {
            return [];
        }
        $out = [];
        foreach ($messages as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $out[] = Normalize::email([
                'id' => $raw['id'] ?? '',
                'from' => $raw['sender'] ?? '',
                'to' => $address,
                'subject' => $raw['subject'] ?? '',
                'text' => $raw['body_text'] ?? '',
                'html' => $raw['body_html'] ?? '',
                'date' => $raw['received_at'] ?? '',
                'attachments' => $raw['attachments'] ?? [],
            ], $address);
        }
        return $out;
    }
}
