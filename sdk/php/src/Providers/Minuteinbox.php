<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\ConfigStore;
use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * minuteinbox.com 渠道实现 — https://www.minuteinbox.com
 *
 * Cookie Session + CSRF + REST JSON（与 fakemail/disposablemail-com 同源结构）：
 *   GET / 获取 PHPSESSID 与内联 CSRF="xxx"；
 *   GET /index/index?csrf_token={csrf} 创建邮箱（返回 {email}）；
 *   GET /index/refresh 拉列表，POST /index/email（form id）拉正文；
 *   token 存 JSON{csrf, cookies}。
 * 捷克语字段：od=from, predmet=subject, telo=html, kdy=date, precteno=已读标记。
 */
final class Minuteinbox
{
    private const ORIGIN = 'https://www.minuteinbox.com';

    private const DEFAULT_UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    private static function userAgent(): string
    {
        foreach (ConfigStore::get()->headers as $k => $v) {
            if (strtolower((string) $k) === 'user-agent' && $v !== '') {
                return (string) $v;
            }
        }
        return self::DEFAULT_UA;
    }

    /**
     * @return array<string,string>
     */
    private static function defaultHeaders(): array
    {
        return [
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'en-US,en;q=0.9',
            'Cache-Control' => 'no-cache',
            'DNT' => '1',
            'Pragma' => 'no-cache',
            'Upgrade-Insecure-Requests' => '1',
            'User-Agent' => self::userAgent(),
        ];
    }

    /**
     * @return array<string,string>
     */
    private static function ajaxHeaders(string $cookie): array
    {
        return [
            'User-Agent' => self::userAgent(),
            'Accept' => 'application/json, text/javascript, */*; q=0.01',
            'Accept-Language' => 'en-US,en;q=0.9',
            'X-Requested-With' => 'XMLHttpRequest',
            'Referer' => self::ORIGIN . '/',
            'Cookie' => $cookie,
        ];
    }

    /**
     * 从响应 Set-Cookie 提取 cookie 映射。
     *
     * @return array<string,string>
     */
    private static function responseCookies(\Psr\Http\Message\ResponseInterface $resp): array
    {
        $out = [];
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first === '' || !str_contains($first, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $first, 2);
            $k = trim($k);
            if ($k !== '') {
                $out[$k] = trim($v);
            }
        }
        return $out;
    }

    /**
     * @param array<string,string> $cookies
     */
    private static function cookiesToStr(array $cookies): string
    {
        ksort($cookies);
        $parts = [];
        foreach ($cookies as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }

    /**
     * @param array<string,string> $cookies
     */
    private static function encodeToken(string $csrf, array $cookies): string
    {
        return (string) json_encode(['csrf' => $csrf, 'cookies' => $cookies], JSON_UNESCAPED_SLASHES);
    }

    /**
     * @return array{0:string,1:array<string,string>}
     */
    private static function decodeToken(?string $token): array
    {
        $data = json_decode((string) $token, true);
        if (!is_array($data)) {
            throw new \InvalidArgumentException('minuteinbox: token 格式无效');
        }
        $csrf = (string) ($data['csrf'] ?? '');
        $cookies = $data['cookies'] ?? null;
        if ($csrf === '' || !is_array($cookies)) {
            throw new \InvalidArgumentException('minuteinbox: token 数据不完整');
        }
        /** @var array<string,string> $cookies */
        return [$csrf, $cookies];
    }

    public static function generate(): EmailInfo
    {
        $r1 = HttpClient::get(self::ORIGIN . '/', self::defaultHeaders());
        $cookies = self::responseCookies($r1);
        if (preg_match('/CSRF\s*=\s*"([^"]+)"/', (string) $r1->getBody(), $m) !== 1) {
            throw new \RuntimeException('minuteinbox: 无法从首页 HTML 提取 CSRF token');
        }
        $csrf = $m[1];

        $r2 = HttpClient::get(
            self::ORIGIN . '/index/index',
            self::ajaxHeaders(self::cookiesToStr($cookies)),
            query: ['csrf_token' => $csrf],
        );
        $cookies = array_merge($cookies, self::responseCookies($r2));
        $data = HttpClient::json($r2);
        $emailAddr = trim((string) ($data['email'] ?? ''));
        if ($emailAddr === '' || !str_contains($emailAddr, '@')) {
            throw new \RuntimeException('minuteinbox: 返回的邮箱地址无效');
        }

        return new EmailInfo('minuteinbox', $emailAddr, self::encodeToken($csrf, $cookies));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('minuteinbox: token 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('minuteinbox: 邮箱地址不能为空');
        }
        [, $cookies] = self::decodeToken($token);
        $cookieStr = self::cookiesToStr($cookies);

        $r = HttpClient::get(self::ORIGIN . '/index/refresh', self::ajaxHeaders($cookieStr));
        $raw = HttpClient::json($r);
        if (!array_is_list($raw)) {
            return [];
        }

        $out = [];
        foreach ($raw as $item) {
            if (!is_array($item)) {
                continue;
            }
            $mailId = $item['id'] ?? null;
            if ($mailId === null) {
                continue;
            }
            $bodyHtml = self::fetchBody($cookieStr, (string) $mailId);
            $isRead = ($item['precteno'] ?? '') !== 'new';
            $raw_email = [
                'id' => (string) $mailId,
                'from' => $item['od'] ?? '',
                'to' => $addr,
                'subject' => $item['predmet'] ?? '',
                'html' => $bodyHtml,
                'date' => $item['kdy'] ?? '',
                'is_read' => $isRead,
            ];
            $out[] = Normalize::email($raw_email, $addr);
        }
        return $out;
    }

    /**
     * POST /index/email 拉取单封 HTML 正文（telo 字段），失败返回空串。
     */
    private static function fetchBody(string $cookieStr, string $mailId): string
    {
        $headers = self::ajaxHeaders($cookieStr);
        $headers['Content-Type'] = 'application/x-www-form-urlencoded';
        try {
            $resp = HttpClient::post(
                self::ORIGIN . '/index/email',
                $headers,
                body: 'id=' . rawurlencode($mailId),
            );
        } catch (\Throwable) {
            return '';
        }
        if ($resp->getStatusCode() !== 200) {
            return '';
        }
        $text = (string) $resp->getBody();
        $detail = json_decode($text, true);
        if (is_array($detail)) {
            return (string) ($detail['telo'] ?? '');
        }
        return trim($text);
    }
}
