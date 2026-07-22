<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Neighbours (neighbours-sh) 渠道实现 — https://neighbours.sh
 *
 * 公共收件箱，固定域名 neighbours.sh，本地随机生成地址，无需注册。
 * GET /inbox/{address} 取列表拿 uid，逐个 GET /inbox/{address}/{uid} 补全正文。
 */
final class NeighboursSh
{
    private const CHANNEL = 'neighbours-sh';
    private const API_BASE = 'https://neighbours.sh/api/v1';
    private const DOMAIN = 'neighbours.sh';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36',
    ];

    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < 10; $i++) {
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
        return HttpClient::json($resp);
    }

    /**
     * @param array<mixed> $detail
     * @return array<mixed>
     */
    private static function flattenMessage(array $detail, string $recipient): array
    {
        return [
            'id' => (string) ($detail['uid'] ?? ''),
            'from' => self::firstAddress($detail['from'] ?? null),
            'to' => self::firstAddress($detail['to'] ?? null) ?: $recipient,
            'subject' => (string) ($detail['subject'] ?? ''),
            'text' => (string) ($detail['text'] ?? ''),
            'html' => (string) ($detail['html'] ?? ''),
            'date' => (string) ($detail['date'] ?? ''),
            'is_read' => false,
            'attachments' => $detail['attachments'] ?? [],
        ];
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

    public static function generate(): EmailInfo
    {
        $email = self::randomLocal() . '@' . self::DOMAIN;
        return new EmailInfo(self::CHANNEL, $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('neighbours-sh: 缺少邮箱地址');
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
            if ($uid === '') {
                continue;
            }
            $detail = self::fetchDetail($address, $uid);
            if ($detail === null) {
                continue;
            }
            $out[] = Normalize::email(self::flattenMessage($detail, $address), $address);
        }
        return $out;
    }
}
