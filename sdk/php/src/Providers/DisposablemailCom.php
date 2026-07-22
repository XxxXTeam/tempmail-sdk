<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * disposablemail.com 渠道实现 — https://www.disposablemail.com（METRONET 后端）
 *
 * 与 fakemail.net / minuteinbox.com 共享 PHP API 结构：
 *   GET / 提取 CSRF="xxx" 与 session cookie；
 *   GET /index/index?csrf_token={csrf} 创建邮箱（返回 {email}）；
 *   GET /index/refresh 拉列表，GET /email/id/{id} 拉 HTML 正文；
 *   token 存 base64(JSON{t:csrf, c:cookie})。
 * 捷克语字段：od=from, predmet=subject, kdy=date, precteno=已读标记。
 */
final class DisposablemailCom
{
    private const BASE_URL = 'https://www.disposablemail.com';
    private const TOK_PREFIX = 'dmc1:';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * @return array<string,string>
     */
    private static function browserHeaders(): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
        ];
    }

    /**
     * @return array<string,string>
     */
    private static function ajaxHeaders(string $cookieHdr): array
    {
        $headers = [
            'User-Agent' => self::UA,
            'Accept' => 'application/json, text/javascript, */*; q=0.01',
            'Accept-Language' => 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
            'X-Requested-With' => 'XMLHttpRequest',
            'Referer' => self::BASE_URL . '/',
        ];
        if ($cookieHdr !== '') {
            $headers['Cookie'] = $cookieHdr;
        }
        return $headers;
    }

    /**
     * 合并已有 cookie 串与响应 Set-Cookie（按键排序）。
     */
    private static function cookieHeader(string $prev, \Psr\Http\Message\ResponseInterface $resp): string
    {
        $d = [];
        foreach (explode(';', $prev) as $part) {
            $p = trim($part);
            if ($p === '' || !str_contains($p, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $p, 2);
            $k = trim($k);
            if ($k !== '') {
                $d[$k] = trim($v);
            }
        }
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first === '' || !str_contains($first, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $first, 2);
            $k = trim($k);
            if ($k !== '') {
                $d[$k] = trim($v);
            }
        }
        ksort($d);
        $parts = [];
        foreach ($d as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }

    private static function encodeToken(string $csrf, string $cookieHdr): string
    {
        $raw = json_encode(['t' => $csrf, 'c' => $cookieHdr], JSON_UNESCAPED_SLASHES);
        return self::TOK_PREFIX . base64_encode((string) $raw);
    }

    /**
     * @return array{0:string,1:string}
     */
    private static function decodeToken(string $token): array
    {
        if (!str_starts_with($token, self::TOK_PREFIX)) {
            throw new \InvalidArgumentException('disposablemail-com: 无效的 token');
        }
        $decoded = base64_decode(substr($token, strlen(self::TOK_PREFIX)), true);
        if ($decoded === false) {
            throw new \InvalidArgumentException('disposablemail-com: 无效的 token');
        }
        $o = json_decode($decoded, true);
        if (!is_array($o)) {
            throw new \InvalidArgumentException('disposablemail-com: 无效的 token');
        }
        $csrf = trim((string) ($o['t'] ?? ''));
        $cookieHdr = trim((string) ($o['c'] ?? ''));
        if ($csrf === '') {
            throw new \InvalidArgumentException('disposablemail-com: token 中缺少 CSRF');
        }
        return [$csrf, $cookieHdr];
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL . '/', self::browserHeaders());
        $cookieHdr = self::cookieHeader('', $resp);
        if (preg_match('/CSRF="([^"]+)"/', (string) $resp->getBody(), $m) !== 1 || $m[1] === '') {
            throw new \RuntimeException('disposablemail-com: 未能从首页提取 CSRF token');
        }
        $csrf = $m[1];

        $resp2 = HttpClient::get(
            self::BASE_URL . '/index/index',
            self::ajaxHeaders($cookieHdr),
            query: ['csrf_token' => $csrf],
        );
        $cookieHdr = self::cookieHeader($cookieHdr, $resp2);
        $data = HttpClient::json($resp2);
        $email = trim((string) ($data['email'] ?? ''));
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('disposablemail-com: 获取到的邮箱地址无效');
        }

        return new EmailInfo('disposablemail-com', $email, self::encodeToken($csrf, $cookieHdr));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('disposablemail-com: 邮箱地址为空');
        }
        [, $cookieHdr] = self::decodeToken(trim((string) $token));

        $resp = HttpClient::get(self::BASE_URL . '/index/refresh', self::ajaxHeaders($cookieHdr));
        $trimmed = trim((string) $resp->getBody());
        if ($trimmed === '0' || $trimmed === '' || $trimmed === '[]') {
            return [];
        }
        $list = json_decode($trimmed, true);
        if (!is_array($list) || !array_is_list($list) || empty($list)) {
            return [];
        }

        $out = [];
        foreach ($list as $item) {
            if (!is_array($item)) {
                continue;
            }
            $mailId = (string) ($item['id'] ?? '');
            $raw = [
                'id' => $mailId,
                'from' => $item['od'] ?? '',
                'to' => $addr,
                'subject' => $item['predmet'] ?? '',
                'html' => self::fetchBody($cookieHdr, $mailId),
                'date' => $item['kdy'] ?? '',
                'isRead' => ($item['precteno'] ?? null) === 'precteno',
            ];
            $out[] = Normalize::email($raw, $addr);
        }
        return $out;
    }

    /**
     * GET /email/id/{id} 拉取单封 HTML 正文（失败返回空串）。
     */
    private static function fetchBody(string $cookieHdr, string $mailId): string
    {
        if ($mailId === '') {
            return '';
        }
        try {
            $resp = HttpClient::get(self::BASE_URL . '/email/id/' . rawurlencode($mailId), self::ajaxHeaders($cookieHdr));
        } catch (\Throwable) {
            return '';
        }
        if ($resp->getStatusCode() < 200 || $resp->getStatusCode() >= 300) {
            return '';
        }
        return (string) $resp->getBody();
    }
}
