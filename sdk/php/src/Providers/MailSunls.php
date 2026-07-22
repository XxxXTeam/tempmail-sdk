<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Mail Sunls 渠道实现 — https://mail.sunls.de
 *
 * 无需 token/session：从 /api/domain 拉取域名列表后本地随机生成地址，
 * 收信通过 GET /api/fetch?to={email}。
 */
final class MailSunls
{
    private const CHANNEL = 'mail-sunls';
    private const BASE = 'https://mail.sunls.de';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json, text/plain, */*',
        'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
        'Cache-Control' => 'no-cache',
        'Pragma' => 'no-cache',
        'Referer' => 'https://mail.sunls.de/',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    private static function randomLocal(int $length = 10): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * 从 API 获取可用域名列表。
     *
     * @return string[]
     */
    private static function fetchDomains(): array
    {
        $resp = HttpClient::get(self::BASE . '/api/domain', self::HEADERS);
        $data = HttpClient::json($resp);
        $out = [];
        foreach ($data as $d) {
            if (is_string($d) && trim($d) !== '') {
                $out[] = trim($d);
            }
        }
        if (empty($out)) {
            throw new \RuntimeException('mail-sunls: 无可用域名');
        }
        return $out;
    }

    public static function generate(): EmailInfo
    {
        $domains = self::fetchDomains();
        $domain = $domains[array_rand($domains)];
        $email = self::randomLocal() . '@' . $domain;
        return new EmailInfo(self::CHANNEL, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('mail-sunls: 邮箱地址不能为空');
        }
        $resp = HttpClient::get(self::BASE . '/api/fetch', self::HEADERS, query: ['to' => $addr]);
        $data = HttpClient::json($resp);
        $out = [];
        foreach ($data as $raw) {
            if (is_array($raw)) {
                $out[] = Normalize::email($raw, $addr);
            }
        }
        return $out;
    }
}
