<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * TempInbox 渠道实现 — https://www.tempinbox.xyz
 *
 * 指定域名时 GET /email/{addr} 校验；否则 GET /email/Random 返回纯文本地址。
 * 收信 GET /messages/{email}。
 */
final class Tempinbox
{
    private const CHANNEL = 'tempinbox';
    private const BASE = 'https://endpoint.tempinbox.xyz';

    /** @var string[] 支持的域名列表 */
    private const DOMAINS = ['tempinbox.xyz', 'thepiratebay.cloud', 'cryptoblad.nl'];

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json, text/plain, */*',
        'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
        'Cache-Control' => 'no-cache',
        'Pragma' => 'no-cache',
        'Referer' => 'https://www.tempinbox.xyz/',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    private static function randomUser(int $length = 10): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        if ($domain !== null && trim($domain) !== '') {
            $d = strtolower(trim($domain));
            if (!in_array($d, self::DOMAINS, true)) {
                throw new \RuntimeException('tempinbox: 域名不可用: ' . $d);
            }
            $addr = self::randomUser() . '@' . $d;
            HttpClient::get(self::BASE . '/email/' . $addr, self::HEADERS);
        } else {
            $resp = HttpClient::get(self::BASE . '/email/Random', self::HEADERS);
            $addr = trim((string) $resp->getBody(), " \t\n\r\0\x0B\"");
        }

        if ($addr === '' || !str_contains($addr, '@')) {
            throw new \RuntimeException('tempinbox: 返回的邮箱地址无效');
        }
        return new EmailInfo(self::CHANNEL, $addr);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('tempinbox: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::BASE . '/messages/' . $addr, self::HEADERS);
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
