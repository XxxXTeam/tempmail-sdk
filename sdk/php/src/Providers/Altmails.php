<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\ConfigStore;
use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * altmails 渠道实现 — https://tempmail.altmails.com
 *
 * Cookie Session + CSRF + REST JSON：
 *   GET / 建立 session 并从 inline script 提取 CSRF（'_token': 'xxx'）；
 *   GET /random-email-address 获取随机邮箱（纯文本）；
 *   token 存 JSON{csrf, cookies}。
 *   POST /fetch-emails/{email} 拉列表，GET /view/{id} 拉 HTML 正文。
 */
final class Altmails
{
    private const BASE_URL = 'https://tempmail.altmails.com';

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
     * @param array<string,string> $extra
     * @return array<string,string>
     */
    private static function buildHeaders(array $extra = []): array
    {
        return [
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'en-US,en;q=0.9',
            'Cache-Control' => 'no-cache',
            'DNT' => '1',
            'Pragma' => 'no-cache',
            'Upgrade-Insecure-Requests' => '1',
            'User-Agent' => self::userAgent(),
        ] + $extra;
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
            throw new \InvalidArgumentException('altmails: token 格式无效');
        }
        $csrf = (string) ($data['csrf'] ?? '');
        $cookies = $data['cookies'] ?? null;
        if ($csrf === '' || !is_array($cookies)) {
            throw new \InvalidArgumentException('altmails: token 数据不完整');
        }
        /** @var array<string,string> $cookies */
        return [$csrf, $cookies];
    }

    public static function generate(): EmailInfo
    {
        $r1 = HttpClient::get(self::BASE_URL . '/', self::buildHeaders());
        $html = (string) $r1->getBody();
        if (preg_match('/[\'"]_token[\'"]\s*:\s*[\'"]([^\'"]+)[\'"]/', $html, $m) !== 1) {
            throw new \RuntimeException('altmails: 无法从首页提取 CSRF token');
        }
        $csrf = $m[1];
        $cookies = self::responseCookies($r1);

        $r2 = HttpClient::get(
            self::BASE_URL . '/random-email-address',
            self::buildHeaders(['Cookie' => self::cookiesToStr($cookies), 'Referer' => self::BASE_URL . '/']),
        );
        $mailbox = trim((string) $r2->getBody());
        if ($mailbox === '' || !str_contains($mailbox, '@')) {
            throw new \RuntimeException('altmails: 返回的邮箱地址无效');
        }
        $cookies = array_merge($cookies, self::responseCookies($r2));

        return new EmailInfo('altmails', $mailbox, self::encodeToken($csrf, $cookies));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('altmails: token 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('altmails: 邮箱地址不能为空');
        }
        [$csrf, $cookies] = self::decodeToken($token);
        $cookieStr = self::cookiesToStr($cookies);

        $resp = HttpClient::post(
            self::BASE_URL . '/fetch-emails/' . rawurlencode($addr),
            self::buildHeaders([
                'Accept' => 'application/json',
                'X-Requested-With' => 'XMLHttpRequest',
                'Cookie' => $cookieStr,
                'Referer' => self::BASE_URL . '/',
                'Content-Type' => 'application/x-www-form-urlencoded',
            ]),
            body: '_token=' . rawurlencode($csrf),
        );
        $messages = HttpClient::json($resp);
        if (!array_is_list($messages) || empty($messages)) {
            return [];
        }

        $out = [];
        foreach ($messages as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $mailId = $msg['id'] ?? '';
            $htmlBody = '';
            if ($mailId !== '' && $mailId !== null) {
                try {
                    $viewResp = HttpClient::get(
                        self::BASE_URL . '/view/' . rawurlencode((string) $mailId),
                        self::buildHeaders(['Cookie' => $cookieStr, 'Referer' => self::BASE_URL . '/']),
                    );
                    if ($viewResp->getStatusCode() === 200) {
                        $htmlBody = (string) $viewResp->getBody();
                    }
                } catch (\Throwable) {
                }
            }
            $raw = [
                'id' => (string) $mailId,
                'from' => $msg['from'] ?? '',
                'subject' => $msg['subject'] ?? '',
                'html' => $htmlBody,
                'to' => $addr,
                'date' => $msg['date'] ?? '',
            ];
            $out[] = Normalize::email($raw, $addr);
        }
        return $out;
    }
}
