<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;

/**
 * Maildrop 渠道实现
 *
 * https://maildrop.cx，GET /api/suffixes.php 拉取域名，GET /api/emails.php 拉取邮件。
 */
final class Maildrop
{
    private const BASE = 'https://maildrop.cx';
    private const EXCLUDED_SUFFIX = 'transformer.edu.kg';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json, text/plain, */*',
        'Referer' => 'https://maildrop.cx/zh-cn/app',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
        'x-requested-with' => 'XMLHttpRequest',
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
     * @return string[]
     */
    private static function fetchSuffixes(): array
    {
        $resp = HttpClient::get(self::BASE . '/api/suffixes.php', self::HEADERS);
        $data = HttpClient::json($resp);
        $ex = strtolower(self::EXCLUDED_SUFFIX);
        $out = [];
        foreach ($data as $x) {
            if (is_string($x)) {
                $t = trim($x);
                if ($t !== '' && strtolower($t) !== $ex) {
                    $out[] = $t;
                }
            }
        }
        if (empty($out)) {
            throw new \RuntimeException('maildrop: 无可用域名');
        }
        return $out;
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        $suffixes = self::fetchSuffixes();
        if ($domain !== null && trim($domain) !== '') {
            $p = strtolower(trim($domain));
            $dom = null;
            foreach ($suffixes as $d) {
                if (strtolower($d) === $p) {
                    $dom = $d;
                    break;
                }
            }
            if ($dom === null) {
                throw new \RuntimeException('maildrop: 域名不可用: ' . $p);
            }
        } else {
            $dom = $suffixes[array_rand($suffixes)];
        }
        $email = self::randomLocal() . '@' . $dom;
        return new EmailInfo('maildrop', $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $addr = trim($email) !== '' ? trim($email) : trim((string) $token);
        if ($addr === '') {
            throw new \InvalidArgumentException('maildrop: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::BASE . '/api/emails.php', self::HEADERS, query: [
            'addr' => $addr,
            'page' => '1',
            'limit' => '20',
        ]);
        $data = HttpClient::json($resp);
        $rows = $data['emails'] ?? [];
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                continue;
            }
            $ir = $row['isRead'] ?? null;
            $out[] = new Email(
                id: (string) ($row['id'] ?? ''),
                fromAddr: trim((string) ($row['from_addr'] ?? '')),
                to: $addr,
                subject: trim((string) ($row['subject'] ?? '')),
                text: trim((string) ($row['description'] ?? '')),
                html: '',
                date: self::cxDateToIso((string) ($row['date'] ?? '')),
                isRead: $ir === true || $ir === 1,
                attachments: [],
            );
        }
        return $out;
    }

    private static function cxDateToIso(string $s): string
    {
        $s = trim($s);
        if (strlen($s) === 19 && $s[10] === ' ') {
            return substr($s, 0, 10) . 'T' . substr($s, 11) . 'Z';
        }
        return $s;
    }
}
