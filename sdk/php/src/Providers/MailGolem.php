<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * MailGolem 渠道实现 — https://mailgolem.com
 *
 * Laravel session + CSRF：
 *   GET / 建立 session 并从隐藏 input 提取 CSRF；
 *   GET /random-email-address 获取随机邮箱；token 存 base64(JSON{csrf, cookies})。
 *   getEmails 重新拉首页刷新 session/CSRF，再 POST /fetch-emails/{email} 拉列表。
 */
final class MailGolem
{
    private const BASE = 'https://mailgolem.com';
    private const TOK_PREFIX = 'mgolem1:';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * @return array<string,string>
     */
    private static function defaultHeaders(): array
    {
        return [
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
            'Cache-Control' => 'no-cache',
            'DNT' => '1',
            'Pragma' => 'no-cache',
            'User-Agent' => self::UA,
        ];
    }

    /**
     * 从响应 Set-Cookie 提取 cookie 映射（仅 name=value）。
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
    private static function cookieHeader(array $cookies): string
    {
        $parts = [];
        foreach ($cookies as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }

    private static function extractCsrf(string $html): string
    {
        if (preg_match('/<input[^>]+name="_token"[^>]+value="([^"]+)"/', $html, $m) !== 1) {
            throw new \RuntimeException('mailgolem: 无法从页面中提取 CSRF token');
        }
        return $m[1];
    }

    /**
     * @param array<string,string> $cookies
     */
    private static function encodeToken(string $csrf, array $cookies): string
    {
        $payload = json_encode(['csrf' => $csrf, 'c' => $cookies], JSON_UNESCAPED_SLASHES);
        return self::TOK_PREFIX . base64_encode((string) $payload);
    }

    public static function generate(): EmailInfo
    {
        $r1 = HttpClient::get(self::BASE . '/', self::defaultHeaders());
        $cookies = self::responseCookies($r1);
        $csrf = self::extractCsrf((string) $r1->getBody());

        $r2 = HttpClient::get(
            self::BASE . '/random-email-address',
            self::defaultHeaders() + ['Referer' => self::BASE . '/', 'Cookie' => self::cookieHeader($cookies)],
        );
        $cookies = array_merge($cookies, self::responseCookies($r2));
        $emailAddr = trim((string) $r2->getBody());
        if ($emailAddr === '' || !str_contains($emailAddr, '@')) {
            throw new \RuntimeException('mailgolem: 获取到无效的邮箱地址');
        }

        return new EmailInfo('mailgolem', $emailAddr, self::encodeToken($csrf, $cookies));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('mailgolem: token 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('mailgolem: 邮箱地址不能为空');
        }

        // 原 session 可能已过期，重新获取 session 与 CSRF
        $r1 = HttpClient::get(self::BASE . '/', self::defaultHeaders());
        $cookies = self::responseCookies($r1);
        $newCsrf = self::extractCsrf((string) $r1->getBody());

        $r2 = HttpClient::post(
            self::BASE . '/fetch-emails/' . rawurlencode($addr),
            [
                'Accept' => 'application/json, text/plain, */*',
                'Accept-Language' => 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
                'Content-Type' => 'application/x-www-form-urlencoded',
                'X-Requested-With' => 'XMLHttpRequest',
                'Referer' => self::BASE . '/',
                'User-Agent' => self::UA,
                'Cookie' => self::cookieHeader($cookies),
            ],
            body: '_token=' . rawurlencode($newCsrf),
        );
        $body = HttpClient::json($r2);
        if (!array_is_list($body)) {
            return [];
        }

        $out = [];
        foreach ($body as $item) {
            if (!is_array($item)) {
                continue;
            }
            $raw = [
                'id' => (string) ($item['id'] ?? ''),
                'from' => $item['from'] ?? '',
                'to' => $addr,
                'subject' => $item['subject'] ?? '',
            ];
            $out[] = Normalize::email($raw, $addr);
        }
        return $out;
    }
}
