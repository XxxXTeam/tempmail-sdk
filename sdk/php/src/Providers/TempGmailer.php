<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * TempGmailer 渠道实现 — https://tempgmailer.com
 *
 * 创建: GET / 获取 CSRF token 与 Laravel session cookie → POST /get-gmail 创建邮箱；
 * 读信: POST /get-inbox 获取收件箱。token 序列化保存 csrfToken 与 cookie 头（JSON）。
 */
final class TempGmailer
{
    private const CHANNEL = 'tempgmailer';
    private const BASE_URL = 'https://tempgmailer.com';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * @return array<string,string>
     */
    private static function apiHeaders(string $csrf, string $cookie): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => 'application/json, text/plain, */*',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
            'Referer' => self::BASE_URL . '/',
            'X-Requested-With' => 'XMLHttpRequest',
            'X-TempGmailer-Auth' => 'frontend',
            'Content-Type' => 'application/json',
            'X-CSRF-TOKEN' => $csrf,
            'Cookie' => $cookie,
        ];
    }

    public static function generate(): EmailInfo
    {
        // 访问首页获取 CSRF token 和 session cookie
        $resp = HttpClient::get(self::BASE_URL, ['User-Agent' => self::UA]);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('tempgmailer: 首页请求失败 ' . $resp->getStatusCode());
        }
        if (preg_match('/<meta\s+name="csrf-token"\s+content="([^"]+)"/', (string) $resp->getBody(), $m) !== 1) {
            throw new \RuntimeException('tempgmailer: 无法提取 CSRF token');
        }
        $csrf = $m[1];
        $cookie = self::cookieHeader($resp);
        if ($cookie === '') {
            throw new \RuntimeException('tempgmailer: 未获取到 session cookie');
        }

        $r = HttpClient::post(
            self::BASE_URL . '/get-gmail',
            self::apiHeaders($csrf, $cookie),
            json: ['refresh' => true, 'adblock' => 0],
        );
        $data = HttpClient::json($r);
        if (empty($data['success'])) {
            throw new \RuntimeException('tempgmailer: 创建邮箱失败: ' . (string) ($data['message'] ?? '未知错误'));
        }
        $inner = $data['data'] ?? [];
        $email = is_array($inner) ? trim((string) ($inner['email'] ?? '')) : '';
        if ($email === '') {
            throw new \RuntimeException('tempgmailer: 创建邮箱失败, 未返回邮箱地址');
        }
        $tokenPayload = (string) json_encode(['csrfToken' => $csrf, 'cookies' => $cookie], JSON_UNESCAPED_SLASHES);
        return new EmailInfo(self::CHANNEL, $email, $tokenPayload);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('tempgmailer: token 不能为空');
        }
        $session = json_decode($token, true);
        if (!is_array($session)) {
            throw new \InvalidArgumentException('tempgmailer: token 格式无效');
        }
        $csrf = (string) ($session['csrfToken'] ?? '');
        $cookie = (string) ($session['cookies'] ?? '');

        $r = HttpClient::post(
            self::BASE_URL . '/get-inbox',
            self::apiHeaders($csrf, $cookie),
            json: ['email' => $email, 'adblock' => 0],
        );
        $data = HttpClient::json($r);
        if (empty($data['success'])) {
            return [];
        }
        $inner = $data['data'] ?? [];
        $messages = is_array($inner) ? ($inner['messages'] ?? null) : null;
        if (!is_array($messages)) {
            return [];
        }

        $out = [];
        foreach ($messages as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $fromField = $msg['from'] ?? '';
            if (is_array($fromField)) {
                $fromField = $fromField['address'] ?? '';
            }
            $htmlContent = ($msg['body'] ?? '') ?: ($msg['intro'] ?? '');
            $textContent = $msg['intro'] ?? '';

            $out[] = Normalize::email([
                'id' => $msg['id'] ?? '',
                'from' => $fromField,
                'to' => $email,
                'subject' => $msg['subject'] ?? '',
                'text' => $textContent,
                'html' => $htmlContent,
                'date' => $msg['createdAt'] ?? '',
            ], $email);
        }
        return $out;
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
        $parts = [];
        foreach ($d as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }
}
