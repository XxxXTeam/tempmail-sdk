<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * MyTempMail.cc 渠道实现 — https://mytempmail.cc
 *
 * POST /api/address 创建邮箱（固定域名 nilvaro.com，有效期 600 秒），响应含 token；
 * GET /api/mails/{token} 拉取邮件（results 数组）。
 */
final class MytempmailCc
{
    private const BASE_URL = 'https://api.mytempmail.cc';
    private const DEFAULT_DOMAIN = 'nilvaro.com';
    private const DEFAULT_EXPIRY = 600;

    /** @var array<string,string> */
    private const HEADERS = [
        'Content-Type' => 'application/json',
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    ];

    private static function randomName(int $length = 10): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(
            self::BASE_URL . '/api/address',
            self::HEADERS,
            json: [
                'domain' => self::DEFAULT_DOMAIN,
                'name' => self::randomName(),
                'expiry' => self::DEFAULT_EXPIRY,
            ],
        );
        $data = HttpClient::json($resp);
        $token = (string) ($data['token'] ?? '');
        $address = (string) ($data['address'] ?? '');
        $expiresIn = is_numeric($data['expires_in'] ?? null) ? (int) $data['expires_in'] : self::DEFAULT_EXPIRY;
        if ($token === '' || $address === '' || !str_contains($address, '@')) {
            throw new \RuntimeException('mytempmail-cc: 创建邮箱失败');
        }
        $expiresAt = (time() + $expiresIn) * 1000;
        return new EmailInfo('mytempmail-cc', $address, $token, expiresAt: $expiresAt);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('mytempmail-cc: token 不能为空');
        }
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('mytempmail-cc: 邮箱地址不能为空');
        }

        $resp = HttpClient::get(self::BASE_URL . '/api/mails/' . rawurlencode($token), self::HEADERS);
        $data = HttpClient::json($resp);
        $results = $data['results'] ?? null;
        if (!is_array($results)) {
            return [];
        }

        $out = [];
        foreach ($results as $item) {
            if (!is_array($item)) {
                continue;
            }
            $raw = [
                'id' => $item['id'] ?? '',
                'from' => $item['from'] ?? ($item['from_address'] ?? ($item['sender'] ?? '')),
                'to' => $item['to'] ?? $address,
                'subject' => $item['subject'] ?? '',
                'text' => $item['text'] ?? ($item['body_text'] ?? ''),
                'html' => $item['html'] ?? ($item['body_html'] ?? ''),
                'date' => $item['date'] ?? ($item['received_at'] ?? ''),
            ];
            $out[] = Normalize::email($raw, $address);
        }
        return $out;
    }
}
