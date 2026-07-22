<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Mail123 渠道实现 — https://mail123.fr
 *
 * 创建 GET /api/v1/mailbox/new 取邮箱地址；读信 GET /mailbox/{addr}/messages?limit=50，
 * 逐封 GET /mailbox/{addr}/messages/{id} 补全正文。Token 即邮箱地址本身。
 */
final class Mail123
{
    private const CHANNEL = 'mail123';
    private const API_BASE = 'https://mail123.fr/api/v1';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::API_BASE . '/mailbox/new', self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('mail123: 创建邮箱失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['address'] ?? ''));
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('mail123: invalid mailbox response');
        }
        $expiresAt = null;
        if (isset($data['expires_in_days']) && is_numeric($data['expires_in_days']) && $data['expires_in_days'] > 0) {
            $expiresAt = (int) ((time() + (float) $data['expires_in_days'] * 86400) * 1000);
        }
        return new EmailInfo(self::CHANNEL, $email, $email, expiresAt: $expiresAt);
    }

    /**
     * 获取单封邮件详情，失败返回 null。
     *
     * @return array<mixed>|null
     */
    private static function fetchDetail(string $address, string $messageId): ?array
    {
        try {
            $resp = HttpClient::get(
                self::API_BASE . '/mailbox/' . rawurlencode($address) . '/messages/' . rawurlencode($messageId),
                self::HEADERS,
            );
            if ($resp->getStatusCode() >= 400) {
                return null;
            }
            $data = HttpClient::json($resp);
            if (is_array($data['message'] ?? null)) {
                return $data['message'];
            }
        } catch (\Throwable) {
            return null;
        }
        return null;
    }

    /**
     * 扁平化消息字段。
     *
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        $out = $raw;
        $out['id'] = $raw['id'] ?? '';
        $out['to'] = $raw['to'] ?? $recipient;
        $out['text'] = $raw['text'] ?? ($raw['preview'] ?? '');
        $out['html'] = $raw['html'] ?? '';
        if (isset($raw['is_unread']) && is_bool($raw['is_unread'])) {
            $out['isRead'] = !$raw['is_unread'];
        }
        return $out;
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('mail123: empty email');
        }
        $resp = HttpClient::get(
            self::API_BASE . '/mailbox/' . rawurlencode($address) . '/messages?limit=50',
            self::HEADERS,
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('mail123: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $rows = $data['messages'] ?? null;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $messageId = trim((string) ($item['id'] ?? ''));
            $detail = $messageId !== '' ? self::fetchDetail($address, $messageId) : null;
            $out[] = Normalize::email(self::flatten($detail ?? $item, $address), $address);
        }
        return $out;
    }
}
