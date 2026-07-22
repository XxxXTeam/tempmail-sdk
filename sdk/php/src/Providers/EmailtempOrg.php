<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * emailtemp.org 渠道实现 — https://emailtemp.org
 *
 * Laravel + CSRF：GET /en 建立 session 并从 meta 提取 CSRF；
 *   POST /messages（body: _token={csrf}&captcha=）返回 {mailbox, messages}；
 *   GET /view/{id} 拉单封 HTML 正文；token 存 base64(JSON{t:csrf, c:cookie})。
 */
final class EmailtempOrg
{
    private const BASE_URL = 'https://emailtemp.org';
    private const TOK_PREFIX = 'eto1:';

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
    private static function ajaxHeaders(): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => 'application/json, text/javascript, */*; q=0.01',
            'Accept-Language' => 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
            'X-Requested-With' => 'XMLHttpRequest',
            'Referer' => self::BASE_URL . '/en',
        ];
    }

    private static function cookieHeader(\Psr\Http\Message\ResponseInterface $resp): string
    {
        $d = [];
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
    private static function decodeToken(?string $token): array
    {
        $token = (string) $token;
        if (!str_starts_with($token, self::TOK_PREFIX)) {
            throw new \InvalidArgumentException('emailtemp-org: 无效的 token');
        }
        $decoded = base64_decode(substr($token, strlen(self::TOK_PREFIX)), true);
        if ($decoded === false) {
            throw new \InvalidArgumentException('emailtemp-org: 无效的 token');
        }
        $o = json_decode($decoded, true);
        if (!is_array($o)) {
            throw new \InvalidArgumentException('emailtemp-org: 无效的 token');
        }
        $csrf = trim((string) ($o['t'] ?? ''));
        $cookieHdr = trim((string) ($o['c'] ?? ''));
        if ($csrf === '') {
            throw new \InvalidArgumentException('emailtemp-org: token 中缺少 CSRF');
        }
        return [$csrf, $cookieHdr];
    }

    /**
     * POST /messages 获取 {mailbox, messages} JSON。
     *
     * @return array<mixed>
     */
    private static function postMessages(string $csrf, string $cookieHdr): array
    {
        $headers = self::ajaxHeaders();
        $headers['Content-Type'] = 'application/x-www-form-urlencoded';
        $headers['X-CSRF-TOKEN'] = $csrf;
        if ($cookieHdr !== '') {
            $headers['Cookie'] = $cookieHdr;
        }
        $resp = HttpClient::post(
            self::BASE_URL . '/messages',
            $headers,
            form: ['_token' => $csrf, 'captcha' => ''],
        );
        $data = HttpClient::json($resp);
        return $data;
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL . '/en', self::browserHeaders());
        $cookieHdr = self::cookieHeader($resp);
        if (preg_match('/<meta\s+name="csrf-token"\s+content="([^"]+)"/', (string) $resp->getBody(), $m) !== 1) {
            throw new \RuntimeException('emailtemp-org: 未能从首页提取 CSRF token');
        }
        $csrf = $m[1];

        $data = self::postMessages($csrf, $cookieHdr);
        $mailbox = trim((string) ($data['mailbox'] ?? ''));
        if ($mailbox === '' || !str_contains($mailbox, '@')) {
            throw new \RuntimeException('emailtemp-org: 邮箱地址无效');
        }
        return new EmailInfo('emailtemp-org', $mailbox, self::encodeToken($csrf, $cookieHdr));
    }

    private static function fetchView(string $cookieHdr, string $mid): string
    {
        if ($mid === '') {
            return '';
        }
        $headers = self::browserHeaders();
        $headers['Referer'] = self::BASE_URL . '/en';
        if ($cookieHdr !== '') {
            $headers['Cookie'] = $cookieHdr;
        }
        try {
            $resp = HttpClient::get(self::BASE_URL . '/view/' . rawurlencode($mid), $headers);
        } catch (\Throwable) {
            return '';
        }
        if ($resp->getStatusCode() < 200 || $resp->getStatusCode() >= 300) {
            return '';
        }
        return (string) $resp->getBody();
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('emailtemp-org: 邮箱地址为空');
        }
        [$csrf, $cookieHdr] = self::decodeToken($token);

        $data = self::postMessages($csrf, $cookieHdr);
        $msgs = $data['messages'] ?? null;
        if (!is_array($msgs) || empty($msgs)) {
            return [];
        }

        $out = [];
        foreach ($msgs as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $mid = trim((string) ($msg['id'] ?? ''));
            if ($mid === '') {
                continue;
            }
            $fromAddr = (string) ($msg['from_email'] ?? '');
            $fromName = (string) ($msg['from'] ?? '');
            if ($fromName !== '' && $fromName !== $fromAddr) {
                $fromAddr = $fromName . ' <' . $fromAddr . '>';
            }
            $raw = [
                'id' => $mid,
                'from' => $fromAddr,
                'to' => $addr,
                'subject' => $msg['subject'] ?? '',
                'html' => self::fetchView($cookieHdr, $mid),
                'isRead' => (bool) ($msg['is_seen'] ?? false),
            ];
            $out[] = Normalize::email($raw, $addr);
        }
        return $out;
    }
}
