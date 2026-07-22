<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * DisposableMail.app 渠道实现 — https://disposablemail.app
 *
 * 纯 REST JSON，无鉴权：POST /api/inbox 创建收件箱，响应含 address 与 token；
 * GET /api/inbox/emails?token={token} 拉取邮件。
 */
final class DisposablemailApp
{
    private const BASE = 'https://disposablemail.app';

    /** @var array<string,string> */
    private const HEADERS = [
        'Content-Type' => 'application/json',
        'Accept' => 'application/json',
        'Referer' => self::BASE . '/',
        'Origin' => self::BASE,
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::BASE . '/api/inbox', self::HEADERS, json: []);
        $body = HttpClient::json($resp);
        $address = (string) ($body['address'] ?? '');
        $token = (string) ($body['token'] ?? '');
        if ($address === '' || !str_contains($address, '@')) {
            throw new \RuntimeException('disposablemail-app: 返回的邮箱地址无效');
        }
        if ($token === '') {
            throw new \RuntimeException('disposablemail-app: 返回的 token 为空');
        }
        return new EmailInfo('disposablemail-app', $address, $token);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('disposablemail-app: token 不能为空');
        }
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('disposablemail-app: 邮箱地址不能为空');
        }

        $resp = HttpClient::get(
            self::BASE . '/api/inbox/emails',
            ['Accept' => 'application/json', 'Referer' => self::BASE . '/'],
            query: ['token' => $token],
        );
        $body = HttpClient::json($resp);
        $rows = $body['emails'] ?? [];
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $raw = [
                'id' => (string) ($item['id'] ?? ''),
                'from' => $item['from'] ?? ($item['from_address'] ?? ($item['sender'] ?? '')),
                'to' => $item['to'] ?? $address,
                'subject' => $item['subject'] ?? '',
                'text' => $item['text'] ?? ($item['body_text'] ?? ''),
                'html' => $item['html'] ?? ($item['body_html'] ?? ($item['body'] ?? '')),
                'date' => $item['date'] ?? ($item['created_at'] ?? ($item['receivedAt'] ?? '')),
            ];
            $out[] = Normalize::email($raw, $address);
        }
        return $out;
    }
}
