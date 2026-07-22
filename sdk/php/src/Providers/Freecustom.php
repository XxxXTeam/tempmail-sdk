<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * freecustom 渠道实现 — https://www.freecustom.email
 *
 * 免注册，Token 即邮箱地址本身。
 *   创建: GET https://api2.freecustom.email/domains 取域名（tier=free 且未临期），本地拼随机 local part
 *   读信: POST /api/auth 取匿名 JWT → GET /api/public-mailbox?fullMailboxId=<email> 列表
 *         → 逐封 GET ...&messageId=<id> 补全正文
 */
final class Freecustom
{
    private const CHANNEL = 'freecustom';
    private const SITE_URL = 'https://www.freecustom.email';
    private const DOMAINS_URL = 'https://api2.freecustom.email/domains';
    private const REFERER = 'https://www.freecustom.email/en';

    private static function userAgent(): string
    {
        $headers = \ChanhanzhanX\TempMail\ConfigStore::get()->headers;
        foreach ($headers as $k => $v) {
            if (strtolower((string) $k) === 'user-agent' && $v !== '') {
                return (string) $v;
            }
        }
        return 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36';
    }

    private static function randomLocal(int $n = 10): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $n; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * @param array<mixed> $msg
     */
    private static function first(array $msg, string ...$keys): string
    {
        foreach ($keys as $key) {
            $val = $msg[$key] ?? null;
            if ($val !== null && $val !== '') {
                return (string) $val;
            }
        }
        return '';
    }

    /**
     * 挑选一个当前可收信域名（优先 tier=free 且 expiring_soon 非 true）。
     */
    private static function pickDomain(): string
    {
        $resp = HttpClient::get(self::DOMAINS_URL, [
            'User-Agent' => self::userAgent(),
            'Accept' => 'application/json',
            'Referer' => self::REFERER,
        ]);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('freecustom: 域名请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $lst = is_array($data['data'] ?? null) ? $data['data'] : null;
        if (!is_array($lst) || $lst === []) {
            throw new \RuntimeException('freecustom: 域名列表为空');
        }
        $usable = [];
        $all = [];
        foreach ($lst as $d) {
            if (!is_array($d) || empty($d['domain'])) {
                continue;
            }
            $all[] = (string) $d['domain'];
            if (($d['tier'] ?? null) === 'free' && ($d['expiring_soon'] ?? null) !== true) {
                $usable[] = (string) $d['domain'];
            }
        }
        $pool = $usable !== [] ? $usable : $all;
        if ($pool === []) {
            throw new \RuntimeException('freecustom: 无可用域名');
        }
        return $pool[array_rand($pool)];
    }

    /**
     * 获取匿名访问令牌（JWT）。
     */
    private static function fetchAuthToken(): string
    {
        $resp = HttpClient::post(self::SITE_URL . '/api/auth', [
            'User-Agent' => self::userAgent(),
            'Accept' => 'application/json',
            'Content-Type' => 'application/json',
            'Referer' => self::REFERER,
        ]);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('freecustom: 令牌请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $jwt = trim((string) ($data['token'] ?? ''));
        if ($jwt === '') {
            throw new \RuntimeException('freecustom: 令牌响应无效');
        }
        return $jwt;
    }

    public static function generate(): EmailInfo
    {
        $domain = self::pickDomain();
        $email = self::randomLocal() . '@' . $domain;
        return new EmailInfo(self::CHANNEL, $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($token ?? '') !== '' ? trim((string) $token) : trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('freecustom: 缺少邮箱地址');
        }
        $jwt = self::fetchAuthToken();
        $authHeaders = [
            'User-Agent' => self::userAgent(),
            'Accept' => 'application/json',
            'Referer' => self::REFERER,
            'Authorization' => 'Bearer ' . $jwt,
            'x-fce-client' => 'web-client',
        ];

        $listResp = HttpClient::get(
            self::SITE_URL . '/api/public-mailbox?fullMailboxId=' . rawurlencode($addr),
            $authHeaders,
        );
        if ($listResp->getStatusCode() >= 400) {
            throw new \RuntimeException('freecustom: 列表请求失败 ' . $listResp->getStatusCode());
        }
        $listData = HttpClient::json($listResp);
        if (empty($listData['success'])) {
            return [];
        }
        $items = $listData['data'] ?? null;
        if (!is_array($items)) {
            return [];
        }

        $out = [];
        foreach ($items as $item) {
            if (!is_array($item) || empty($item['id'])) {
                continue;
            }
            $msgId = (string) $item['id'];
            $full = $item;
            try {
                $msgResp = HttpClient::get(
                    self::SITE_URL . '/api/public-mailbox?fullMailboxId=' . rawurlencode($addr)
                        . '&messageId=' . rawurlencode($msgId),
                    $authHeaders,
                );
                if ($msgResp->getStatusCode() < 400) {
                    $msgData = HttpClient::json($msgResp);
                    if (!empty($msgData['success']) && is_array($msgData['data'] ?? null)) {
                        $full = $msgData['data'];
                    }
                }
            } catch (\Throwable) {
                // 正文补全失败时退回列表元数据
            }
            $out[] = Normalize::email([
                'id' => self::first($full, 'id'),
                'from' => self::first($full, 'from'),
                'to' => self::first($full, 'to') ?: $addr,
                'subject' => self::first($full, 'subject'),
                'text' => self::first($full, 'text'),
                'html' => self::first($full, 'html'),
                'date' => self::first($full, 'date'),
            ], $addr);
        }
        return $out;
    }
}
