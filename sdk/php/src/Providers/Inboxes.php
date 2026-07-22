<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Inboxes 渠道实现 — https://inboxes.com
 *
 * 创建：GET /api/v2/domain 取域名（qdn），本地拼随机 local part；
 * 读信：GET /api/v2/inbox/{email} 取 msgs，逐封 GET /api/v2/message/{uid} 补全正文。
 */
final class Inboxes
{
    private const CHANNEL = 'inboxes';
    private const API_BASE = 'https://inboxes.com/api/v2';
    private const DEFAULT_DOMAIN = 'blondmail.com';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'Origin' => 'https://inboxes.com',
        'Referer' => 'https://inboxes.com/',
        'User-Agent' => 'Mozilla/5.0',
    ];

    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = 'sdk';
        for ($i = 0; $i < 16; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * @return string[]
     */
    private static function getDomains(): array
    {
        $resp = HttpClient::get(self::API_BASE . '/domain', self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('inboxes: 域名请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $items = is_array($data['domains'] ?? null) ? $data['domains'] : [];
        $domains = [];
        foreach ($items as $item) {
            if (is_array($item)) {
                $qdn = trim((string) ($item['qdn'] ?? ''));
                if ($qdn !== '') {
                    $domains[] = $qdn;
                }
            }
        }
        if ($domains === []) {
            throw new \RuntimeException('inboxes: no domains');
        }
        return $domains;
    }

    /**
     * @param string[] $domains
     */
    private static function pickDomain(array $domains, ?string $preferred): string
    {
        $wanted = strtolower(ltrim(trim((string) $preferred), '@'));
        if ($wanted !== '') {
            foreach ($domains as $domain) {
                if (strtolower($domain) === $wanted) {
                    return $domain;
                }
            }
        }
        if (in_array(self::DEFAULT_DOMAIN, $domains, true)) {
            return self::DEFAULT_DOMAIN;
        }
        return $domains[0];
    }

    /**
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        return [
            'id' => $raw['uid'] ?? '',
            'from' => $raw['sf'] ?? ($raw['f'] ?? ''),
            'to' => $raw['ib'] ?? $recipient,
            'subject' => $raw['s'] ?? '',
            'text' => $raw['text'] ?? '',
            'html' => $raw['html'] ?? '',
            'preview_text' => $raw['ph'] ?? '',
            'date' => $raw['cr'] ?? '',
            'isRead' => (bool) ($raw['ru'] ?? false),
            'attachments' => is_array($raw['at'] ?? null) ? $raw['at'] : [],
        ];
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        $domains = self::getDomains();
        $selected = self::pickDomain($domains, $domain);
        return new EmailInfo(self::CHANNEL, self::randomLocal() . '@' . $selected);
    }

    /**
     * @return array<mixed>|null
     */
    private static function fetchDetail(string $uid): ?array
    {
        try {
            $resp = HttpClient::get(self::API_BASE . '/message/' . rawurlencode($uid), self::HEADERS);
            if ($resp->getStatusCode() >= 400) {
                return null;
            }
            $data = HttpClient::json($resp);
            return $data !== [] ? $data : null;
        } catch (\Throwable) {
            return null;
        }
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('inboxes: empty email');
        }
        $resp = HttpClient::get(self::API_BASE . '/inbox/' . rawurlencode($address), self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('inboxes: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $rows = $data['msgs'] ?? null;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                continue;
            }
            $uid = trim((string) ($row['uid'] ?? ''));
            $detail = $uid !== '' ? self::fetchDetail($uid) : null;
            $out[] = Normalize::email(self::flatten($detail ?? $row, $address), $address);
        }
        return $out;
    }
}
