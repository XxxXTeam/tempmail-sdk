<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Neighbours 渠道实现 — https://neighbours.sh
 *
 * 从 /config/domains 拉取域名列表随机（或按指定）选域名本地生成地址；
 * GET /inbox/{address} 列表，逐 uid GET /inbox/{address}/{uid} 补全详情。
 */
final class Neighbours
{
    private const CHANNEL = 'neighbours';
    private const API_BASE = 'https://neighbours.sh/api/v1';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36',
    ];

    /**
     * 请求 JSON；allow404 时 404 返回 null。
     *
     * @return array<mixed>|null
     */
    private static function requestJson(string $path, bool $allow404 = false): ?array
    {
        $resp = HttpClient::get(self::API_BASE . $path, self::HEADERS);
        if ($allow404 && $resp->getStatusCode() === 404) {
            return null;
        }
        $data = HttpClient::json($resp);
        return $data;
    }

    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < 16; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return 'sdk' . $out;
    }

    /**
     * 从 from/to 结构中提取首个可用地址。
     */
    private static function firstAddress(mixed $value): string
    {
        if ($value === null) {
            return '';
        }
        if (is_string($value)) {
            return trim($value);
        }
        if (is_array($value)) {
            if (array_is_list($value)) {
                foreach ($value as $item) {
                    $hit = self::firstAddress($item);
                    if ($hit !== '') {
                        return $hit;
                    }
                }
                return '';
            }
            $address = trim((string) ($value['address'] ?? ''));
            if ($address !== '') {
                return $address;
            }
            $text = trim((string) ($value['text'] ?? ''));
            if (str_contains($text, '@')) {
                return $text;
            }
            return self::firstAddress($value['value'] ?? null);
        }
        return trim((string) $value);
    }

    /**
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flattenMessage(array $raw, string $recipient): array
    {
        return [
            'id' => (string) ($raw['uid'] ?? ''),
            'from' => self::firstAddress($raw['from'] ?? null),
            'to' => self::firstAddress($raw['to'] ?? null) ?: $recipient,
            'subject' => (string) ($raw['subject'] ?? ''),
            'text' => (string) ($raw['text'] ?? $raw['snippet'] ?? ''),
            'html' => (string) ($raw['html'] ?? ''),
            'date' => (string) ($raw['date'] ?? $raw['received_at'] ?? ''),
            'is_read' => false,
            'attachments' => $raw['attachments'] ?? [],
        ];
    }

    /**
     * @return string[]
     */
    private static function listDomains(): array
    {
        $data = self::requestJson('/config/domains');
        $domains = [];
        $nested = is_array($data) ? ($data['data'] ?? null) : null;
        if (is_array($nested) && is_array($nested['domains'] ?? null)) {
            foreach ($nested['domains'] as $item) {
                $itemStr = strtolower(trim((string) $item));
                if ($itemStr !== '') {
                    $domains[] = $itemStr;
                }
            }
        }
        if (empty($domains)) {
            throw new \RuntimeException('neighbours: 域名列表为空');
        }
        return $domains;
    }

    /**
     * @param string[] $domains
     */
    private static function pickDomain(array $domains, ?string $preferred): string
    {
        if ($preferred !== null && trim($preferred) !== '') {
            $wanted = strtolower(trim($preferred));
            if (!in_array($wanted, $domains, true)) {
                throw new \RuntimeException('neighbours: 不支持的域名 ' . $preferred);
            }
            return $wanted;
        }
        return $domains[array_rand($domains)];
    }

    /**
     * @return array<mixed>|null
     */
    private static function fetchDetail(string $address, string $uid): ?array
    {
        $data = self::requestJson('/inbox/' . rawurlencode($address) . '/' . rawurlencode($uid), true);
        if (is_array($data) && is_array($data['data'] ?? null)) {
            return $data['data'];
        }
        return null;
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        $domains = self::listDomains();
        $selected = self::pickDomain($domains, $domain);
        $email = self::randomLocal() . '@' . $selected;
        return new EmailInfo(self::CHANNEL, $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('neighbours: 邮箱地址为空');
        }
        $data = self::requestJson('/inbox/' . rawurlencode($address), true);
        if ($data === null) {
            return [];
        }
        $rows = $data['data'] ?? null;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $uid = trim((string) ($item['uid'] ?? ''));
            $detail = $uid !== '' ? self::fetchDetail($address, $uid) : null;
            $out[] = Normalize::email(self::flattenMessage($detail ?? $item, $address), $address);
        }
        return $out;
    }
}
