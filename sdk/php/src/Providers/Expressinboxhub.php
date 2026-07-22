<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * ExpressInboxHub 渠道实现 — https://expressinboxhub.com
 *
 * CSRF + session cookie：GET / 提取 meta csrf-token 与 session cookie；
 *   POST /messages（JSON {_token}）返�� {mailbox, messages}；
 *   token 存 JSON{csrfToken, cookies}，读信时复用同一 POST。
 */
final class Expressinboxhub
{
    private const BASE = 'https://expressinboxhub.com';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * @return array<string,string>
     */
    private static function headers(): array
    {
        return [
            'Accept' => 'application/json, text/plain, */*',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
            'Content-Type' => 'application/json',
            'Origin' => self::BASE,
            'Referer' => self::BASE . '/',
            'User-Agent' => self::UA,
        ];
    }

    /**
     * GET 首页，提取 CSRF token 与 session cookie 串。
     *
     * @return array{0:string,1:string}
     */
    private static function initSession(): array
    {
        $resp = HttpClient::get(self::BASE, ['User-Agent' => self::UA]);
        if (preg_match('/<meta\s+name=["\']csrf-token["\']\s+content=["\']([^"\']+)["\']/', (string) $resp->getBody(), $m) !== 1) {
            throw new \RuntimeException('expressinboxhub: 无法提取 CSRF token');
        }
        $csrf = $m[1];

        $cookies = [];
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first === '' || !str_contains($first, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $first, 2);
            $k = trim($k);
            if ($k !== '') {
                $cookies[] = $k . '=' . trim($v);
            }
        }
        $cookieStr = implode('; ', $cookies);
        if ($cookieStr === '') {
            throw new \RuntimeException('expressinboxhub: 未获取到 session cookies');
        }
        return [$csrf, $cookieStr];
    }

    /**
     * POST /messages 返回 {mailbox, messages} JSON。
     *
     * @return array<mixed>
     */
    private static function postMessages(string $csrf, string $cookies): array
    {
        $resp = HttpClient::post(
            self::BASE . '/messages',
            self::headers() + ['X-CSRF-TOKEN' => $csrf, 'Cookie' => $cookies],
            json: ['_token' => $csrf],
        );
        return HttpClient::json($resp);
    }

    public static function generate(): EmailInfo
    {
        [$csrf, $cookies] = self::initSession();
        $data = self::postMessages($csrf, $cookies);
        $mailbox = trim((string) ($data['mailbox'] ?? ''));
        if ($mailbox === '') {
            throw new \RuntimeException('expressinboxhub: 响应中缺少 mailbox 字段');
        }
        $tokenPayload = (string) json_encode(['csrfToken' => $csrf, 'cookies' => $cookies], JSON_UNESCAPED_SLASHES);
        return new EmailInfo('expressinboxhub', $mailbox, $tokenPayload);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('expressinboxhub: 缺少 token');
        }
        $sessionData = json_decode($token, true);
        if (!is_array($sessionData)) {
            throw new \InvalidArgumentException('expressinboxhub: token 格式无效');
        }
        $csrf = (string) ($sessionData['csrfToken'] ?? '');
        $cookies = (string) ($sessionData['cookies'] ?? '');

        $data = self::postMessages($csrf, $cookies);
        $messages = $data['messages'] ?? null;
        if (!is_array($messages)) {
            return [];
        }

        $out = [];
        foreach ($messages as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $normalized = $raw;
            if (isset($normalized['receivedAt']) && !isset($normalized['date'])) {
                $normalized['date'] = $normalized['receivedAt'];
            }
            if (isset($normalized['content']) && !isset($normalized['html'])) {
                $normalized['html'] = $normalized['content'];
            }
            $out[] = Normalize::email($normalized, $email);
        }
        return $out;
    }
}
