<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * 1SecMail 渠道实现 — https://tmaily.com
 *
 * GET /generate 创建邮箱，从 Set-Cookie 提取 TMaily_sid 作为会话；
 * GET /emails?address={email} 携带 Cookie 轮询邮件。
 */
final class OneSecMail
{
    private const BASE_URL = 'https://tmaily.com/';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0',
    ];

    /**
     * 从响应 Set-Cookie 中提取 TMaily_sid 会话 Cookie 头。
     */
    private static function extractCookie(\Psr\Http\Message\ResponseInterface $resp): string
    {
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            if (preg_match('/TMaily_sid=([^;]+)/', $sc, $m) === 1) {
                return 'TMaily_sid=' . $m[1];
            }
        }
        return '';
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL . 'generate', self::HEADERS);
        $cookie = self::extractCookie($resp);
        if ($cookie === '') {
            throw new \RuntimeException('1sec-mail: 未获取到会话 Cookie');
        }
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['address'] ?? ''));
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('1sec-mail: 无效的邮箱响应');
        }
        return new EmailInfo('1sec-mail', $email, $cookie);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('1sec-mail: 邮箱地址为空');
        }
        $resp = HttpClient::get(
            self::BASE_URL . 'emails?address=' . rawurlencode($address),
            self::HEADERS + ['Cookie' => (string) $token],
        );
        $rows = HttpClient::json($resp);
        if (!array_is_list($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $item) {
            if (is_array($item)) {
                $out[] = Normalize::email($item, $address);
            }
        }
        return $out;
    }
}
