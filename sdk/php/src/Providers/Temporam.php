<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Temporam 渠道实现 — https://temporam.com
 *
 * GET /api/domains 拉域名随机（或指定）选域名本地生成地址（token 存地址）；
 * GET /api/emails?email=&limit=50 列表，逐条 GET /api/emails/{id} 补全详情。
 */
final class Temporam
{
    private const CHANNEL = 'temporam';
    private const ORIGIN = 'https://temporam.com';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'Accept-Encoding' => 'gzip, deflate, br',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
    ];

    private static function randomLocal(int $length = 18): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return 'sdk' . $out;
    }

    /**
     * @return array<mixed>
     */
    private static function getJson(string $path): array
    {
        $resp = HttpClient::get(self::ORIGIN . $path, self::HEADERS);
        return HttpClient::json($resp);
    }

    /**
     * @return string[]
     */
    private static function domains(): array
    {
        $data = self::getJson('/api/domains');
        $rows = $data['data'] ?? [];
        $domains = [];
        if (is_array($rows)) {
            foreach ($rows as $item) {
                if (is_array($item)) {
                    $d = trim((string) ($item['domain'] ?? ''));
                    if ($d !== '') {
                        $domains[] = $d;
                    }
                }
            }
        }
        if (empty($domains)) {
            throw new \RuntimeException('temporam: 域名列表为空');
        }
        return $domains;
    }

    /**
     * @param array<mixed> $raw
     */
    private static function normalize(array $raw, string $email): Email
    {
        $flat = $raw;
        $flat['id'] = $raw['id'] ?? $raw['uuid'] ?? '';
        $flat['from'] = $raw['fromEmail'] ?? $raw['from'] ?? '';
        $flat['to'] = $raw['toEmail'] ?? $raw['to'] ?? $email;
        $flat['date'] = $raw['createdAt'] ?? $raw['created_at'] ?? $raw['date'] ?? '';
        return Normalize::email($flat, $email);
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        $domains = self::domains();
        $selected = $domains[array_rand($domains)];
        if ($domain !== null && $domain !== '') {
            if (!in_array($domain, $domains, true)) {
                throw new \RuntimeException('temporam: 不支持的域名 ' . $domain);
            }
            $selected = $domain;
        }
        $email = self::randomLocal() . '@' . $selected;
        return new EmailInfo(self::CHANNEL, $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $recipient = ($token !== null && $token !== '') ? $token : $email;
        if ($recipient === '') {
            throw new \RuntimeException('temporam: 缺少邮箱地址');
        }
        $data = self::getJson('/api/emails?email=' . rawurlencode($recipient) . '&limit=50');
        $rows = $data['data'] ?? [];
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                continue;
            }
            $raw = $row;
            $msgId = $row['id'] ?? $row['uuid'] ?? '';
            if ($msgId !== '' && $msgId !== null) {
                try {
                    $detail = self::getJson('/api/emails/' . rawurlencode((string) $msgId));
                    if (is_array($detail['data'] ?? null)) {
                        $raw = $detail['data'];
                    }
                } catch (\Throwable) {
                    $raw = $row;
                }
            }
            $out[] = self::normalize($raw, $recipient);
        }
        return $out;
    }
}
