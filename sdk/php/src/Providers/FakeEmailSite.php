<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * FakeEmailSite 渠道实现 — https://fake-email.site
 *
 * 创建 POST /api/temporary-address（空 JSON），取 temp_email_addr；
 * 读信 GET /api/inbox/poll?address=xxx，取 messages 数组。Token 即邮箱地址本身。
 */
final class FakeEmailSite
{
    private const CHANNEL = 'fake-email-site';
    private const BASE = 'https://api.fake-email.site';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'Content-Type' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::BASE . '/api/temporary-address', self::HEADERS, json: []);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('fake-email-site: 创建邮箱失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['temp_email_addr'] ?? ''));
        if ($email === '') {
            throw new \RuntimeException('fake-email-site: 响应中未找到临时邮箱地址');
        }
        return new EmailInfo(self::CHANNEL, $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('fake-email-site: empty email');
        }
        $resp = HttpClient::get(self::BASE . '/api/inbox/poll?address=' . rawurlencode($address), self::HEADERS);
        if ($resp->getStatusCode() === 404) {
            return [];
        }
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('fake-email-site: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $rows = $data['messages'] ?? null;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $row) {
            if (is_array($row)) {
                $out[] = Normalize::email($row, $address);
            }
        }
        return $out;
    }
}
