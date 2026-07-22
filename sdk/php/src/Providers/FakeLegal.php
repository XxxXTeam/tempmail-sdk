<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Fake Legal 渠道实现（含 imgui.de / pulsewebmenu.de 别名）
 *
 * https://imgui.de，通过 /api/inbox 创建收件箱。
 * imgui.de / pulsewebmenu.de 用 POST /api/inbox/custom 保根域，
 * 其余域名用 GET /api/inbox/new 创建。
 */
final class FakeLegal
{
    private const BASE = 'https://imgui.de';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json, text/plain, */*',
        'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
        'Cache-Control' => 'no-cache',
        'DNT' => '1',
        'Pragma' => 'no-cache',
        'Referer' => 'https://imgui.de/',
        'sec-ch-ua' => '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
        'sec-ch-ua-mobile' => '?0',
        'sec-ch-ua-platform' => '"Windows"',
        'sec-fetch-dest' => 'empty',
        'sec-fetch-mode' => 'cors',
        'sec-fetch-site' => 'same-origin',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    /**
     * @return string[]
     */
    private static function fetchDomains(): array
    {
        $resp = HttpClient::get(self::BASE . '/api/domains', self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            return [];
        }
        $data = HttpClient::json($resp);
        $raw = $data['domains'] ?? null;
        if (!is_array($raw)) {
            return [];
        }
        $out = [];
        foreach ($raw as $d) {
            if (is_string($d) && trim($d) !== '') {
                $out[] = trim($d);
            }
        }
        return $out;
    }

    /**
     * @param string[] $domains
     */
    private static function pickDomain(array $domains, ?string $preferred): string
    {
        $p = trim((string) $preferred);
        if ($p !== '') {
            $pl = strtolower($p);
            foreach ($domains as $d) {
                if (strtolower($d) === $pl) {
                    return $d;
                }
            }
            throw new \RuntimeException('fake-legal: 域名不可用: ' . $p);
        }
        return $domains[array_rand($domains)];
    }

    private static function randomUsername(int $length): string
    {
        $chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(?string $domain = null, string $channel = 'fake-legal'): EmailInfo
    {
        $selectedChannel = trim($channel) ?: 'fake-legal';
        $domains = self::fetchDomains();
        if (empty($domains)) {
            throw new \RuntimeException('fake-legal: 无可用域名');
        }
        $d = self::pickDomain($domains, $domain);

        if ($d === 'imgui.de' || $d === 'pulsewebmenu.de') {
            // imgui-de / pulsewebmenu-de 用 POST 保根域
            $resp = HttpClient::post(
                self::BASE . '/api/inbox/custom',
                self::HEADERS,
                json: ['username' => self::randomUsername(12), 'domain' => $d],
            );
        } else {
            // fake-legal 用 GET 创建
            $resp = HttpClient::get(
                self::BASE . '/api/inbox/new?domain=' . rawurlencode($d),
                self::HEADERS,
            );
        }
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('fake-legal: 创建收件箱失败');
        }
        $data = HttpClient::json($resp);
        if (empty($data['success'])) {
            throw new \RuntimeException('fake-legal: 创建收件箱失败');
        }
        $addr = trim((string) ($data['address'] ?? ''));
        if ($addr === '') {
            throw new \RuntimeException('fake-legal: 缺少 address');
        }
        return new EmailInfo($selectedChannel, $addr, $addr);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $e = trim($email);
        if ($e === '') {
            throw new \InvalidArgumentException('fake-legal: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::BASE . '/api/inbox/' . rawurlencode($e), self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('fake-legal: 拉取邮件失败');
        }
        $data = HttpClient::json($resp);
        if (empty($data['success'])) {
            return [];
        }
        $rows = $data['emails'] ?? null;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $out[] = Normalize::email($raw, $e);
        }
        return $out;
    }
}
