<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * tempmail.fish 渠道实现
 *
 * API: https://api.tempmail.fish
 * 流程: POST /emails/new-email 创建邮箱（email + authKey，token 序列化为 JSON）→
 *       GET /emails/emails?emailAddress={email} 读信，认证头 Authorization: {authKey}（无 Bearer 前缀）。
 */
final class TempMailFish
{
    private const CHANNEL = 'tempmail-fish';
    private const API_BASE = 'https://api.tempmail.fish';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(
            self::API_BASE . '/emails/new-email',
            self::HEADERS + ['Content-Type' => 'application/json'],
        );
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['email'] ?? ''));
        $authKey = trim((string) ($data['authKey'] ?? ''));
        if ($email === '' || !str_contains($email, '@') || $authKey === '') {
            throw new \RuntimeException('tempmail-fish: 创建邮箱响应无效');
        }
        $token = (string) json_encode(['email' => $email, 'authKey' => $authKey], JSON_UNESCAPED_SLASHES);
        return new EmailInfo(self::CHANNEL, $email, $token);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('tempmail-fish: token 不能为空');
        }
        $parsed = json_decode($token, true);
        if (!is_array($parsed)) {
            throw new \InvalidArgumentException('tempmail-fish: token 格式无效');
        }
        $address = trim((string) (($parsed['email'] ?? '') ?: $email));
        $authKey = trim((string) ($parsed['authKey'] ?? ''));
        if ($address === '' || $authKey === '') {
            throw new \InvalidArgumentException('tempmail-fish: token 缺少 email 或 authKey');
        }

        $resp = HttpClient::get(
            self::API_BASE . '/emails/emails?emailAddress=' . rawurlencode($address),
            self::HEADERS + ['Authorization' => $authKey],
        );
        $data = HttpClient::json($resp);
        if (array_is_list($data)) {
            $rows = $data;
        } elseif (is_array($data['emails'] ?? null)) {
            $rows = $data['emails'];
        } else {
            $rows = [];
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
     * @param array<mixed> $raw
     * @return array<string,mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        return [
            'from' => (string) ($raw['from'] ?? ''),
            'to' => (string) ($raw['to'] ?? '') ?: $recipient,
            'subject' => (string) ($raw['subject'] ?? ''),
            // textBody 中常内嵌 HTML，交由 Normalize 自动识别归位
            'text' => (string) ($raw['textBody'] ?? ''),
            'html' => (string) ($raw['htmlBody'] ?? ''),
            'date' => self::toIso($raw['timestamp'] ?? null),
            'is_read' => false,
            'attachments' => $raw['attachments'] ?? [],
        ];
    }

    /**
     * 将毫秒时间戳转换为 ISO8601 字符串，非法值返回空串。
     */
    private static function toIso(mixed $timestamp): string
    {
        if (!is_numeric($timestamp)) {
            return '';
        }
        $millis = (float) $timestamp;
        return date('c', (int) ($millis / 1000.0));
    }
}
