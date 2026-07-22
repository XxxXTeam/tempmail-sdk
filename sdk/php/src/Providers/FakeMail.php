<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * FakeMail 渠道实现 — https://www.fakemail.net
 *
 * PHP 后端 + CSRF：GET / 提取内联脚本中的 const CSRF="xxx" 与 session cookie；
 *   GET /index/index?csrf_token={csrf} 创建邮箱（返回 {email}）；
 *   GET /index/refresh 拉列表，POST /index/email（form id）拉单封详情；
 *   token 直接存合并后的 cookie 串（read 时复用）。
 * 捷克语字段：od=from, predmet=subject, telo=html, kdy=date, precteno=已读标记。
 */
final class FakeMail
{
    private const BASE_URL = 'https://www.fakemail.net';

    /**
     * 合并已有 cookie 串与响应 Set-Cookie（后者覆盖前者）。
     */
    private static function mergeCookie(string $prev, \Psr\Http\Message\ResponseInterface $resp): string
    {
        $jar = [];
        foreach (explode(';', $prev) as $part) {
            $p = trim($part);
            if ($p === '' || !str_contains($p, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $p, 2);
            $jar[trim($k)] = trim($v);
        }
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first === '' || !str_contains($first, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $first, 2);
            $k = trim($k);
            if ($k !== '') {
                $jar[$k] = trim($v);
            }
        }
        $parts = [];
        foreach ($jar as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }

    /**
     * @return array<string,string>
     */
    private static function homeHeaders(): array
    {
        return [
            'Accept' => 'text/html,application/xhtml+xml',
            'User-Agent' => 'Mozilla/5.0',
        ];
    }

    /**
     * @return array<string,string>
     */
    private static function ajaxHeaders(string $cookie): array
    {
        return [
            'Accept' => 'application/json, text/javascript, */*; q=0.01',
            'X-Requested-With' => 'XMLHttpRequest',
            'Referer' => self::BASE_URL . '/',
            'Cookie' => $cookie,
            'User-Agent' => 'Mozilla/5.0',
        ];
    }

    /**
     * 解析可能带 BOM 前缀的 JSON 文本。
     *
     * @return mixed
     */
    private static function decode(string $text)
    {
        return json_decode(ltrim($text, "\u{FEFF}"), true);
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL . '/', self::homeHeaders());
        if (preg_match('/const\s+CSRF\s*=\s*"([^"]+)"/', (string) $resp->getBody(), $m) !== 1) {
            throw new \RuntimeException('fakemail: 未找到 CSRF token');
        }
        $csrf = $m[1];
        $cookie = self::mergeCookie('', $resp);

        $resp2 = HttpClient::get(
            self::BASE_URL . '/index/index',
            self::ajaxHeaders($cookie),
            query: ['csrf_token' => $csrf],
        );
        $cookie = self::mergeCookie($cookie, $resp2);
        $data = self::decode((string) $resp2->getBody());
        if (!is_array($data)) {
            throw new \RuntimeException('fakemail: 创建邮箱响应格式异常');
        }
        $email = trim((string) ($data['email'] ?? ''));
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('fakemail: 获取到的邮箱地址无效');
        }
        return new EmailInfo('fakemail', $email, $cookie);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $cookie = trim((string) $token);
        $addr = trim($email);
        if ($cookie === '') {
            throw new \InvalidArgumentException('fakemail: 缺少 session token');
        }
        if ($addr === '') {
            throw new \InvalidArgumentException('fakemail: 邮箱地址为空');
        }

        $resp = HttpClient::get(self::BASE_URL . '/index/refresh', self::ajaxHeaders($cookie));
        $rows = self::decode((string) $resp->getBody());
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                continue;
            }
            $mid = trim((string) ($row['id'] ?? ''));
            $detail = $mid !== '' ? self::fetchDetail($cookie, $mid) : null;
            $raw = [
                'id' => ($detail['id'] ?? null) ?: ($row['id'] ?? ''),
                'from' => ($detail['od'] ?? null) ?: ($row['od'] ?? ''),
                'to' => $addr,
                'subject' => ($detail['predmet'] ?? null) ?: ($row['predmet'] ?? ''),
                'text' => '',
                'html' => $detail['telo'] ?? '',
                'date' => $row['kdy'] ?? '',
                'isRead' => ($row['precteno'] ?? null) === 'precteno',
            ];
            $out[] = Normalize::email($raw, $addr);
        }
        return $out;
    }

    /**
     * POST /index/email 拉取单封详情（失败返回 null）。
     *
     * @return array<mixed>|null
     */
    private static function fetchDetail(string $cookie, string $mid): ?array
    {
        try {
            $resp = HttpClient::post(
                self::BASE_URL . '/index/email',
                self::ajaxHeaders($cookie),
                form: ['id' => $mid],
            );
        } catch (\Throwable) {
            return null;
        }
        if ($resp->getStatusCode() < 200 || $resp->getStatusCode() >= 300) {
            return null;
        }
        $data = self::decode((string) $resp->getBody());
        return is_array($data) ? $data : null;
    }
}
