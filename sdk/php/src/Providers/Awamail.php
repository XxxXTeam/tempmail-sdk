<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * awamail.com 渠道实现 — https://awamail.com/welcome
 *
 * POST /change_mailbox 创建邮箱，从 Set-Cookie 提取 awamail_session 作为会话；
 * GET /get_emails 携带 Cookie 拉取邮件。
 */
final class Awamail
{
    private const BASE_URL = 'https://awamail.com/welcome';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0',
        'Accept' => 'application/json, text/javascript, */*; q=0.01',
        'accept-language' => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
        'cache-control' => 'no-cache',
        'dnt' => '1',
        'origin' => 'https://awamail.com',
        'pragma' => 'no-cache',
        'referer' => 'https://awamail.com/?lang=zh',
        'sec-ch-ua' => '"Not(A:Brand";v="8", "Chromium";v="144", "Microsoft Edge";v="144"',
        'sec-ch-ua-mobile' => '?0',
        'sec-ch-ua-platform' => '"Windows"',
        'sec-fetch-dest' => 'empty',
        'sec-fetch-mode' => 'cors',
        'sec-fetch-site' => 'same-origin',
    ];

    /**
     * 从响应 Set-Cookie 中提取 awamail_session 会话 Cookie 头。
     */
    private static function extractCookie(\Psr\Http\Message\ResponseInterface $resp): string
    {
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            if (preg_match('/awamail_session=([^;]+)/', $sc, $m) === 1) {
                return 'awamail_session=' . $m[1];
            }
        }
        return '';
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(
            self::BASE_URL . '/change_mailbox',
            self::HEADERS + ['Content-Length' => '0'],
        );
        $cookie = self::extractCookie($resp);
        if ($cookie === '') {
            throw new \RuntimeException('awamail: 未获取到会话 Cookie');
        }
        $data = HttpClient::json($resp);
        $inner = $data['data'] ?? null;
        if (empty($data['success']) || !is_array($inner)) {
            throw new \RuntimeException('awamail: 创建邮箱失败');
        }
        return new EmailInfo(
            'awamail',
            (string) ($inner['email_address'] ?? ''),
            $cookie,
            createdAt: $inner['created_at'] ?? null,
        );
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $resp = HttpClient::get(
            self::BASE_URL . '/get_emails',
            self::HEADERS + [
                'Cookie' => (string) $token,
                'x-requested-with' => 'XMLHttpRequest',
            ],
        );
        $data = HttpClient::json($resp);
        $inner = $data['data'] ?? null;
        if (empty($data['success']) || !is_array($inner)) {
            throw new \RuntimeException('awamail: 获取邮件失败');
        }
        $rows = $inner['emails'] ?? [];
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $raw) {
            if (is_array($raw)) {
                $out[] = Normalize::email($raw, $email);
            }
        }
        return $out;
    }
}
