<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\ConfigStore;
use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * tempemails.net 渠道实现 — https://tempemails.net
 *
 * Laravel Cookie Session + CSRF。
 * 创建: GET / 获取 session 与 CSRF → POST /get_messages 取自动分配邮箱（token 序列化为 JSON）；
 * 读信: POST /get_messages 取 messages → GET /view/{id} 取正文。
 */
final class TempEmailsNet
{
    private const CHANNEL = 'tempemails-net';
    private const BASE_URL = 'https://tempemails.net';

    private const DEFAULT_UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    private static function ua(): string
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
        $h = [
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'en-US,en;q=0.9',
            'Cache-Control' => 'no-cache',
            'DNT' => '1',
            'Pragma' => 'no-cache',
            'Upgrade-Insecure-Requests' => '1',
            'User-Agent' => self::ua(),
        ];
        return $extra + $h;
    }

    public static function generate(): EmailInfo
    {
        $r = HttpClient::get(self::BASE_URL . '/', self::buildHeaders());
        if ($r->getStatusCode() >= 400) {
            throw new \RuntimeException('tempemails-net: 首页请求失败 ' . $r->getStatusCode());
        }
        if (preg_match('/<meta\s+name="csrf-token"\s+content="([^"]+)"/', (string) $r->getBody(), $m) !== 1) {
            throw new \RuntimeException('tempemails-net: 无法从首页提取 CSRF token');
        }
        $csrf = $m[1];
        $cookies = self::collectCookies([], $r);
        $cookieStr = self::cookiesToStr($cookies);

        $r2 = HttpClient::post(
            self::BASE_URL . '/get_messages',
            self::buildHeaders([
                'Accept' => 'application/json',
                'X-Requested-With' => 'XMLHttpRequest',
                'X-CSRF-TOKEN' => $csrf,
                'Cookie' => $cookieStr,
                'Referer' => self::BASE_URL . '/',
            ]),
        );
        $data = HttpClient::json($r2);
        if (empty($data['status'])) {
            throw new \RuntimeException('tempemails-net: 获取邮箱失败');
        }
        $mailbox = trim((string) ($data['mailbox'] ?? ''));
        if ($mailbox === '' || !str_contains($mailbox, '@')) {
            throw new \RuntimeException('tempemails-net: 返回的邮箱地址无效');
        }
        $merged = self::collectCookies($cookies, $r2);
        return new EmailInfo(self::CHANNEL, $mailbox, self::encodeToken($csrf, $merged));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('tempemails-net: token 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('tempemails-net: 邮箱地址不能为空');
        }
        [$csrf, $cookies] = self::decodeToken($token);
        $cookieStr = self::cookiesToStr($cookies);

        $r = HttpClient::post(
            self::BASE_URL . '/get_messages',
            self::buildHeaders([
                'Accept' => 'application/json',
                'X-Requested-With' => 'XMLHttpRequest',
                'X-CSRF-TOKEN' => $csrf,
                'Cookie' => $cookieStr,
                'Referer' => self::BASE_URL . '/',
            ]),
        );
        $data = HttpClient::json($r);
        $messages = $data['messages'] ?? null;
        if (!is_array($messages) || empty($messages)) {
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

            $fromName = trim((string) ($msg['from'] ?? ''));
            $fromAddr = trim((string) ($msg['from_email'] ?? ''));
            $fromDisplay = ($fromName !== '' && $fromAddr !== '')
                ? $fromName . ' <' . $fromAddr . '>'
                : ($fromAddr !== '' ? $fromAddr : $fromName);

            $out[] = Normalize::email([
                'id' => (string) $mailId,
                'from' => $fromDisplay,
                'from_address' => $fromAddr,
                'from_name' => $fromName,
                'to' => $addr,
                'subject' => $msg['subject'] ?? '',
                'html' => $htmlBody,
                'date' => $msg['receivedAt'] ?? '',
            ], $addr);
        }
        return $out;
    }

    /**
     * 合并响应 Set-Cookie 到已有映射。
     *
     * @param array<string,string> $prev
     * @return array<string,string>
     */
    private static function collectCookies(array $prev, ResponseInterface $resp): array
    {
        $d = $prev;
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
        return $d;
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
    private static function decodeToken(string $token): array
    {
        $data = json_decode($token, true);
        if (!is_array($data)) {
            throw new \InvalidArgumentException('tempemails-net: token 格式无效');
        }
        $csrf = (string) ($data['csrf'] ?? '');
        $cookies = $data['cookies'] ?? null;
        if ($csrf === '' || !is_array($cookies)) {
            throw new \InvalidArgumentException('tempemails-net: token 数据不完整');
        }
        return [$csrf, $cookies];
    }
}
