<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * Temp-Mail.Now 渠道实现 — https://temp-mail.now
 *
 * 基于会话 Cookie：GET /en/ 获取 session cookie → POST /change_email 创建邮箱；
 * GET /fetch_emails 读信。token 保存 session cookie 头。
 */
final class TempMailNow
{
    private const CHANNEL = 'temp-mail-now';
    private const BASE_URL = 'https://temp-mail.now';

    /** @var array<string,string> */
    private const PAGE_HEADERS = [
        'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    ];

    /** @var array<string,string> */
    private const API_HEADERS = [
        'Accept' => 'application/json, text/javascript, */*; q=0.01',
        'X-Requested-With' => 'XMLHttpRequest',
        'Referer' => self::BASE_URL . '/en/',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    ];

    public static function generate(): EmailInfo
    {
        // 获取 session cookie
        $resp = HttpClient::get(self::BASE_URL . '/en/', self::PAGE_HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('temp-mail-now: 首页请求失败 ' . $resp->getStatusCode());
        }
        $cookie = self::mergeCookie('', $resp);
        if ($cookie === '') {
            throw new \RuntimeException('temp-mail-now: 无法获取 session cookie');
        }

        // 创建新邮箱
        $resp2 = HttpClient::post(
            self::BASE_URL . '/change_email',
            self::API_HEADERS + ['Cookie' => $cookie],
        );
        $cookie = self::mergeCookie($cookie, $resp2);
        $data = HttpClient::json($resp2);

        $address = (string) ($data['new_email'] ?? '');
        if ($address === '' || !str_contains($address, '@')) {
            throw new \RuntimeException('temp-mail-now: 创建邮箱失败');
        }

        return new EmailInfo(self::CHANNEL, $address, $cookie);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('temp-mail-now: session cookie 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('temp-mail-now: 邮箱地址不能为空');
        }

        $resp = HttpClient::get(self::BASE_URL . '/fetch_emails', self::API_HEADERS + ['Cookie' => $token]);
        $data = HttpClient::json($resp);
        $emailsList = $data['emails'] ?? null;
        if (!is_array($emailsList)) {
            return [];
        }

        $out = [];
        foreach ($emailsList as $item) {
            if (!is_array($item)) {
                continue;
            }
            $raw = [
                'id' => $item['id'] ?? '',
                'from' => ($item['from'] ?? '') ?: (($item['from_address'] ?? '') ?: ($item['sender'] ?? '')),
                'to' => $item['to'] ?? $addr,
                'subject' => $item['subject'] ?? '',
                'text' => ($item['text'] ?? '') ?: ($item['body_text'] ?? ''),
                'html' => ($item['html'] ?? '') ?: ($item['body_html'] ?? ''),
                'date' => ($item['date'] ?? '') ?: ($item['received_at'] ?? ''),
            ];
            $out[] = Normalize::email($raw, $addr);
        }
        return $out;
    }

    /**
     * 将响应 Set-Cookie 合并进已有 Cookie 头。
     */
    private static function mergeCookie(string $prev, ResponseInterface $resp): string
    {
        $d = [];
        foreach (explode(';', $prev) as $part) {
            $part = trim($part);
            if ($part !== '' && str_contains($part, '=')) {
                [$k, $v] = explode('=', $part, 2);
                $d[trim($k)] = trim($v);
            }
        }
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first !== '' && str_contains($first, '=')) {
                [$k, $v] = explode('=', $first, 2);
                $k = trim($k);
                if ($k !== '') {
                    $d[$k] = trim($v);
                }
            }
        }
        $parts = [];
        foreach ($d as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }
}
