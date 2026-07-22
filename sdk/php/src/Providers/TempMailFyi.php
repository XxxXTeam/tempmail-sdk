<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * temp-mail.fyi 渠道实现 — https://temp-mail.fyi
 *
 * 创建: GET / 获取 PHPSESSID cookie 与 CSRF token（正则 csrfToken" value="xxx"）→
 *       POST /api/generate_email.php 创建邮箱；
 * 读信: POST /api/get_emails.php（body 携带 email_address）。
 * token 序列化保存 CSRF 与 cookie 头（base64）。
 */
final class TempMailFyi
{
    private const CHANNEL = 'tempmail-fyi';
    private const BASE_URL = 'https://temp-mail.fyi';
    private const TOK_PREFIX = 'tmf1:';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

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
    private static function apiHeaders(string $csrf, string $cookieHdr): array
    {
        $headers = [
            'User-Agent' => self::UA,
            'Accept' => 'application/json, text/javascript, */*; q=0.01',
            'Accept-Language' => 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
            'Content-Type' => 'application/json',
            'X-CSRF-Token' => $csrf,
            'X-Requested-With' => 'XMLHttpRequest',
            'Referer' => self::BASE_URL . '/',
        ];
        if ($cookieHdr !== '') {
            $headers['Cookie'] = $cookieHdr;
        }
        return $headers;
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL . '/', self::browserHeaders());
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('tempmail-fyi: 首页请求失败 ' . $resp->getStatusCode());
        }
        $cookieHdr = self::cookieHeader($resp);
        if (preg_match('/csrfToken"\s*value="([^"]+)"/', (string) $resp->getBody(), $m) !== 1 || $m[1] === '') {
            throw new \RuntimeException('tempmail-fyi: 未能从首页提取 CSRF token');
        }
        $csrf = $m[1];

        $resp2 = HttpClient::post(
            self::BASE_URL . '/api/generate_email.php',
            self::apiHeaders($csrf, $cookieHdr),
            body: '{}',
        );
        $data = HttpClient::json($resp2);
        if (empty($data['success'])) {
            $err = trim((string) ($data['error'] ?? ''));
            throw new \RuntimeException('tempmail-fyi: 创建邮箱失败' . ($err !== '' ? '：' . $err : '（success=false）'));
        }
        $email = trim((string) ($data['email_address'] ?? ''));
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('tempmail-fyi: 获取到的邮箱地址无效');
        }
        $exp = $data['expires_at'] ?? null;
        return new EmailInfo(
            self::CHANNEL,
            $email,
            self::encodeToken($csrf, $cookieHdr),
            expiresAt: is_numeric($exp) ? (int) $exp : null,
        );
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $email = trim($email);
        if ($email === '') {
            throw new \InvalidArgumentException('tempmail-fyi: 邮箱地址为空');
        }
        [$csrf, $cookieHdr] = self::decodeToken(trim((string) $token));

        $resp = HttpClient::post(
            self::BASE_URL . '/api/get_emails.php',
            self::apiHeaders($csrf, $cookieHdr),
            body: (string) json_encode(['email_address' => $email]),
        );
        $data = HttpClient::json($resp);
        if (empty($data['success'])) {
            $err = trim((string) ($data['error'] ?? ''));
            throw new \RuntimeException('tempmail-fyi: 获取邮件列表失败' . ($err !== '' ? '：' . $err : '（success=false）'));
        }
        $emails = $data['emails'] ?? null;
        if (!is_array($emails) || empty($emails)) {
            return [];
        }

        $out = [];
        foreach ($emails as $item) {
            if (is_array($item)) {
                $out[] = Normalize::email($item, $email);
            }
        }
        return $out;
    }

    private static function encodeToken(string $csrf, string $cookieHdr): string
    {
        $raw = (string) json_encode(['t' => $csrf, 'c' => $cookieHdr], JSON_UNESCAPED_SLASHES);
        return self::TOK_PREFIX . base64_encode($raw);
    }

    /**
     * @return array{0:string,1:string}
     */
    private static function decodeToken(string $token): array
    {
        if (!str_starts_with($token, self::TOK_PREFIX)) {
            throw new \InvalidArgumentException('tempmail-fyi: 无效的 token');
        }
        $decoded = base64_decode(substr($token, strlen(self::TOK_PREFIX)), true);
        $o = $decoded !== false ? json_decode($decoded, true) : null;
        if (!is_array($o)) {
            throw new \InvalidArgumentException('tempmail-fyi: 无效的 token');
        }
        $csrf = trim((string) ($o['t'] ?? ''));
        $cookieHdr = trim((string) ($o['c'] ?? ''));
        if ($csrf === '') {
            throw new \InvalidArgumentException('tempmail-fyi: token 中缺少 CSRF');
        }
        return [$csrf, $cookieHdr];
    }

    private static function cookieHeader(ResponseInterface $resp): string
    {
        $d = [];
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
        ksort($d);
        $parts = [];
        foreach ($d as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }
}
