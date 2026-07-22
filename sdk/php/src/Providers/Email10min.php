<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * email10min 渠道实现 — https://email10min.com
 *
 * Laravel + CSRF：GET /zh 提取 meta/input CSRF、页面内邮箱地址与 session cookie；
 *   POST /messages?{ts}（form _token={csrf}&captcha=）返回 {messages}；
 *   token 存 base64(JSON{c:cookie, t:csrf})。
 */
final class Email10min
{
    private const BASE_URL = 'https://email10min.com';
    private const TOK_PREFIX = 'e10m:';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * @return array<string,string>
     */
    private static function browserHeaders(): array
    {
        return [
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
            'Cache-Control' => 'no-cache',
            'DNT' => '1',
            'Pragma' => 'no-cache',
            'Upgrade-Insecure-Requests' => '1',
            'User-Agent' => self::UA,
            'sec-ch-ua' => '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
            'sec-ch-ua-mobile' => '?0',
            'sec-ch-ua-platform' => '"Windows"',
        ];
    }

    /**
     * @return array<string,string>
     */
    private static function ajaxHeaders(string $cookie): array
    {
        return [
            'Accept' => 'application/json, text/plain, */*',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
            'Content-Type' => 'application/x-www-form-urlencoded; charset=UTF-8',
            'Origin' => self::BASE_URL,
            'Referer' => self::BASE_URL . '/zh',
            'User-Agent' => self::UA,
            'X-Requested-With' => 'XMLHttpRequest',
            'sec-ch-ua' => '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
            'sec-ch-ua-mobile' => '?0',
            'sec-ch-ua-platform' => '"Windows"',
            'sec-fetch-dest' => 'empty',
            'sec-fetch-mode' => 'cors',
            'sec-fetch-site' => 'same-origin',
            'Cookie' => $cookie,
        ];
    }

    private static function cookieHeader(\Psr\Http\Message\ResponseInterface $resp): string
    {
        $parts = [];
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first === '' || !str_contains($first, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $first, 2);
            $k = trim($k);
            if ($k !== '') {
                $parts[] = $k . '=' . trim($v);
            }
        }
        return implode('; ', $parts);
    }

    private static function encodeToken(string $cookie, string $csrf): string
    {
        $raw = json_encode(['c' => $cookie, 't' => $csrf], JSON_UNESCAPED_SLASHES);
        return self::TOK_PREFIX . base64_encode((string) $raw);
    }

    /**
     * @return array{0:string,1:string}
     */
    private static function decodeToken(?string $token): array
    {
        $token = (string) $token;
        if (!str_starts_with($token, self::TOK_PREFIX)) {
            throw new \InvalidArgumentException('email10min: invalid session token');
        }
        $raw = base64_decode(substr($token, strlen(self::TOK_PREFIX)), true);
        if ($raw === false) {
            throw new \InvalidArgumentException('email10min: invalid session token');
        }
        $data = json_decode($raw, true);
        if (!is_array($data)) {
            throw new \InvalidArgumentException('email10min: invalid session token');
        }
        $cookie = trim((string) ($data['c'] ?? ''));
        $csrf = trim((string) ($data['t'] ?? ''));
        if ($cookie === '' || $csrf === '') {
            throw new \InvalidArgumentException('email10min: invalid session token (empty fields)');
        }
        return [$cookie, $csrf];
    }

    private static function extractCsrf(string $html): string
    {
        if (preg_match('/<meta\s+name="csrf-token"\s+content="([^"]+)"/i', $html, $m) === 1) {
            return $m[1];
        }
        if (preg_match('/<input[^>]+name="_token"[^>]+value="([^"]+)"/i', $html, $m) === 1) {
            return $m[1];
        }
        throw new \RuntimeException('email10min: 未找到 CSRF token');
    }

    private static function extractEmail(string $html): string
    {
        if (preg_match('/id="emailAddress"[^>]*>([^<]+)/i', $html, $m) === 1 && str_contains($m[1], '@')) {
            return trim($m[1]);
        }
        if (preg_match('/class="[^"]*email[^"]*"[^>]*>([^<]*@[^<]+)/i', $html, $m) === 1) {
            return trim($m[1]);
        }
        if (preg_match('/data-email="([^"]+@[^"]+)"/i', $html, $m) === 1) {
            return trim($m[1]);
        }
        if (preg_match('/value="([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})"/', $html, $m) === 1) {
            return trim($m[1]);
        }
        if (preg_match('/"mailbox"\s*:\s*"([^"]+@[^"]+)"/', $html, $m) === 1) {
            return trim($m[1]);
        }
        if (preg_match('/([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})/', $html, $m) === 1) {
            $addr = $m[1];
            if (!str_contains($addr, 'email10min') && !str_contains($addr, 'example') && !str_contains($addr, 'google')) {
                return trim($addr);
            }
        }
        throw new \RuntimeException('email10min: 未找到邮箱地址');
    }

    public static function generate(): EmailInfo
    {
        $r = HttpClient::get(self::BASE_URL . '/zh', self::browserHeaders());
        $cookie = self::cookieHeader($r);
        $html = (string) $r->getBody();
        $csrf = self::extractCsrf($html);
        $emailAddr = self::extractEmail($html);
        return new EmailInfo('email10min', $emailAddr, self::encodeToken($cookie, $csrf));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        [$cookie, $csrf] = self::decodeToken($token);
        $ts = (string) (int) (microtime(true) * 1000);

        $r = HttpClient::post(
            self::BASE_URL . '/messages?' . $ts,
            self::ajaxHeaders($cookie),
            body: '_token=' . rawurlencode($csrf) . '&captcha=',
        );
        $data = HttpClient::json($r);
        $messages = $data['messages'] ?? null;
        if (!is_array($messages)) {
            return [];
        }

        $out = [];
        foreach ($messages as $i => $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $msgId = ($raw['id'] ?? null) ?: (($raw['message_id'] ?? null) ?: (string) $i);
            $flat = [
                'id' => (string) $msgId,
                'from' => ($raw['from'] ?? null) ?: ($raw['sender'] ?? ''),
                'to' => ($raw['to'] ?? null) ?: $email,
                'subject' => $raw['subject'] ?? '',
                'text' => ($raw['text'] ?? null) ?: ($raw['body'] ?? ''),
                'html' => ($raw['html'] ?? null) ?: ($raw['body_html'] ?? ''),
                'date' => ($raw['date'] ?? null) ?: ($raw['created_at'] ?? ''),
                'isRead' => false,
            ];
            $out[] = Normalize::email($flat, $email);
        }
        return $out;
    }
}
