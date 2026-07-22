<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * FMail 渠道实现 — https://fmail.men
 *
 * 创建 GET /v1/random（可选 ?domain=），取 address；
 * 读信 GET /v1/inbox/{local}?domain=xxx&limit=50，逐封 GET /v1/email/{token} 补全正文。
 */
final class Fmail
{
    private const CHANNEL = 'fmail';
    private const BASE_URL = 'https://fmail.men';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
    ];

    /**
     * @return array<mixed>
     */
    private static function fetchJson(string $path): array
    {
        $resp = HttpClient::get(self::BASE_URL . $path, self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('fmail: 请求失败 ' . $resp->getStatusCode());
        }
        return HttpClient::json($resp);
    }

    private static function normalizeDomain(?string $domain): ?string
    {
        $value = ltrim(trim((string) $domain), '@');
        return $value !== '' ? $value : null;
    }

    /**
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        return [
            'id' => $raw['id'] ?? ($raw['uid'] ?? ($raw['token'] ?? '')),
            'from' => $raw['sender'] ?? ($raw['from'] ?? ''),
            'to' => $raw['recipient'] ?? ($raw['to'] ?? $recipient),
            'subject' => $raw['subject'] ?? '',
            'text' => $raw['body_text'] ?? ($raw['text'] ?? ($raw['snippet'] ?? '')),
            'html' => $raw['body_html'] ?? ($raw['html'] ?? ''),
            'date' => $raw['received_at'] ?? ($raw['timestamp'] ?? ($raw['date'] ?? '')),
            'is_read' => $raw['is_read'] ?? false,
            'attachments' => $raw['attachments'] ?? [],
        ];
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        $selected = self::normalizeDomain($domain);
        $path = '/v1/random';
        if ($selected !== null) {
            $path .= '?domain=' . rawurlencode($selected);
        }
        $data = self::fetchJson($path);
        $email = trim((string) ($data['address'] ?? ''));
        if ($email === '') {
            $username = trim((string) ($data['username'] ?? ''));
            $dom = trim((string) ($data['domain'] ?? ''));
            if ($username !== '' && $dom !== '') {
                $email = $username . '@' . $dom;
            }
        }
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('fmail: invalid random response');
        }
        return new EmailInfo(self::CHANNEL, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $value = trim($email);
        $parts = explode('@', $value, 2);
        $local = $parts[0] ?? '';
        $domain = $parts[1] ?? '';
        if ($local === '' || $domain === '') {
            throw new \InvalidArgumentException('fmail: invalid email');
        }
        $data = self::fetchJson(
            '/v1/inbox/' . rawurlencode($local) . '?domain=' . rawurlencode($domain) . '&limit=50'
        );
        $rows = $data['emails'] ?? null;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                continue;
            }
            $token = trim((string) ($row['token'] ?? ($row['id'] ?? '')));
            if ($token === '') {
                $out[] = Normalize::email(self::flatten($row, $value), $value);
                continue;
            }
            try {
                $detail = self::fetchJson('/v1/email/' . rawurlencode($token));
                $nested = is_array($detail['email'] ?? null) ? $detail['email'] : $detail;
                $out[] = Normalize::email(self::flatten($nested, $value), $value);
            } catch (\Throwable) {
                $out[] = Normalize::email(self::flatten($row, $value), $value);
            }
        }
        return $out;
    }
}
