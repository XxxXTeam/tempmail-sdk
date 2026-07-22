<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Anonymmail 渠道实现 — https://anonymmail.net
 *
 * POST 表单 API：先取域名，本地拼随机用户名，POST /api/create 创建；
 * 读信 POST /api/get，响应形如 {"email@domain": {"emails": [...]}}。
 */
final class Anonymmail
{
    private const CHANNEL = 'anonymmail';
    private const BASE = 'https://anonymmail.net';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => '*/*',
        'Content-Type' => 'application/x-www-form-urlencoded',
        'Referer' => 'https://anonymmail.net/',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    private static function randomUsername(int $length = 10): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * 获取可用域名列表。
     *
     * @return string[]
     */
    private static function fetchDomains(): array
    {
        $resp = HttpClient::post(self::BASE . '/api/getDomains', self::HEADERS);
        $data = HttpClient::json($resp);
        if (!array_is_list($data)) {
            return [];
        }
        $out = [];
        foreach ($data as $item) {
            if (is_array($item)) {
                $domain = trim((string) ($item['domain'] ?? ''));
                if ($domain !== '') {
                    $out[] = $domain;
                }
            }
        }
        return $out;
    }

    public static function generate(): EmailInfo
    {
        $domains = self::fetchDomains();
        if ($domains === []) {
            throw new \RuntimeException('anonymmail: no domains available');
        }
        $domain = $domains[array_rand($domains)];
        $email = self::randomUsername() . '@' . $domain;

        // HEAD 请求获取 cookie session（本端共享客户端自动持有 Cookie）
        HttpClient::get(self::BASE . '/', self::HEADERS);

        $resp = HttpClient::post(self::BASE . '/api/create', self::HEADERS, body: 'email=' . $email);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('anonymmail: create inbox failed ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        if (empty($data['success'])) {
            throw new \RuntimeException('anonymmail: create inbox failed');
        }
        $addr = trim((string) ($data['email'] ?? ''));
        if ($addr === '') {
            throw new \RuntimeException('anonymmail: missing email in response');
        }
        return new EmailInfo(self::CHANNEL, $addr);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $e = trim($email);
        if ($e === '') {
            throw new \InvalidArgumentException('anonymmail: empty email');
        }
        $resp = HttpClient::post(self::BASE . '/api/get', self::HEADERS, body: 'email=' . $e);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('anonymmail: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        // 响应格式: {"email@domain": {"created_at": "...", "emails": [...]}}
        $inbox = $data[$e] ?? null;
        if (!is_array($inbox)) {
            return [];
        }
        $rows = $inbox['emails'] ?? null;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $normalized = $raw;
            // token 字段映射为 id
            if (isset($normalized['token']) && !isset($normalized['id'])) {
                $normalized['id'] = $normalized['token'];
                unset($normalized['token']);
            }
            // body 字段映射为 html（body 含 HTML 内容）
            if (isset($normalized['body']) && !isset($normalized['html'])) {
                $normalized['html'] = $normalized['body'];
                unset($normalized['body']);
            }
            $out[] = Normalize::email($normalized, $e);
        }
        return $out;
    }
}
