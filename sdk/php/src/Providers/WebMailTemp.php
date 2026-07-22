<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * webmailtemp.com 渠道实现
 *
 * API: https://webmailtemp.com
 * 流程: GET /api/create 创建邮箱（email + username，并保存 Set-Cookie）→
 *       GET /api/check/{username} 读信（携带 Cookie）。
 * token 序列化保存 username 与 Cookie 头（base64）。
 */
final class WebMailTemp
{
    private const CHANNEL = 'webmailtemp';
    private const BASE_URL = 'https://webmailtemp.com';
    private const TOK_PREFIX = 'wmt1:';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL . '/api/create', self::HEADERS);
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['email'] ?? ''));
        $username = trim((string) ($data['username'] ?? ''));
        $cookieHeader = self::firstCookie($resp);
        if (
            empty($data['success'])
            || $email === ''
            || !str_contains($email, '@')
            || $username === ''
            || $cookieHeader === ''
        ) {
            throw new \RuntimeException('webmailtemp: 创建邮箱响应无效');
        }
        $ttl = $data['ttl'] ?? null;
        $expiresAt = (is_numeric($ttl) && (float) $ttl > 0)
            ? (int) ((time() + (float) $ttl) * 1000)
            : null;
        return new EmailInfo(self::CHANNEL, $email, self::encodeToken($username, $cookieHeader), expiresAt: $expiresAt);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('webmailtemp: 邮箱地址不能为空');
        }
        $session = self::decodeToken((string) $token);

        $resp = HttpClient::get(
            self::BASE_URL . '/api/check/' . rawurlencode($session['username']),
            self::HEADERS + ['Cookie' => $session['cookie']],
        );
        $data = HttpClient::json($resp);
        $rows = $data['emails'] ?? null;
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $row) {
            if (is_array($row)) {
                $out[] = Normalize::email(self::flatten($row, $address), $address);
            }
        }
        return $out;
    }

    /**
     * 提取响应中的首个 Set-Cookie（name=value）。
     */
    private static function firstCookie(ResponseInterface $resp): string
    {
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first !== '' && str_contains($first, '=')) {
                return $first;
            }
        }
        return '';
    }

    private static function encodeToken(string $username, string $cookie): string
    {
        $raw = (string) json_encode(['u' => $username, 'c' => $cookie], JSON_UNESCAPED_SLASHES);
        return self::TOK_PREFIX . base64_encode($raw);
    }

    /**
     * @return array{username:string,cookie:string}
     */
    private static function decodeToken(string $token): array
    {
        if (!str_starts_with($token, self::TOK_PREFIX)) {
            throw new \InvalidArgumentException('webmailtemp: 无效的 token');
        }
        $decoded = base64_decode(substr($token, strlen(self::TOK_PREFIX)), true);
        if ($decoded === false) {
            throw new \InvalidArgumentException('webmailtemp: 无效的 token');
        }
        $data = json_decode($decoded, true);
        $username = is_array($data) ? trim((string) ($data['u'] ?? '')) : '';
        $cookie = is_array($data) ? trim((string) ($data['c'] ?? '')) : '';
        if ($username === '' || $cookie === '') {
            throw new \InvalidArgumentException('webmailtemp: token 数据不完整');
        }
        return ['username' => $username, 'cookie' => $cookie];
    }

    /**
     * @param array<mixed> $raw
     * @return array<string,mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        return [
            'id' => $raw['id'] ?? '',
            'from' => $raw['from'] ?? '',
            'to' => $recipient,
            'subject' => $raw['subject'] ?? '',
            'text' => $raw['body'] ?? '',
            'html' => $raw['html'] ?? '',
            'date' => ($raw['received_at'] ?? '') ?: ($raw['timestamp'] ?? ''),
            'attachments' => $raw['attachments'] ?? [],
        ];
    }
}
