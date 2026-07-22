<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * tempmailpro.us 渠道实现
 *
 * API: https://tempmailpro.us/api/v1
 * 流程: POST /mailbox/create 创建邮箱（data.address + data.token）→
 *       GET /mailbox/{token}/emails 列表 → GET /mailbox/{token}/emails/{id} 详情。
 */
final class TempMailPro
{
    private const CHANNEL = 'tempmailpro';
    private const API_BASE = 'https://tempmailpro.us/api/v1';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(
            self::API_BASE . '/mailbox/create',
            self::HEADERS + ['Content-Type' => 'application/json'],
            json: [],
        );
        $data = HttpClient::json($resp);
        $box = $data['data'] ?? null;
        if (!is_array($box)) {
            throw new \RuntimeException('tempmailpro: 创建邮箱响应无效');
        }
        $email = trim((string) ($box['address'] ?? ''));
        $token = trim((string) ($box['token'] ?? ''));
        if ($email === '' || !str_contains($email, '@') || $token === '') {
            throw new \RuntimeException('tempmailpro: 创建邮箱响应无效');
        }
        $exp = $box['expires_at'] ?? null;
        return new EmailInfo(
            self::CHANNEL,
            $email,
            $token,
            expiresAt: is_numeric($exp) ? (int) $exp : null,
            createdAt: $box['created_at'] ?? null,
        );
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $mailboxToken = trim((string) $token);
        $address = trim($email);
        if ($mailboxToken === '') {
            throw new \InvalidArgumentException('tempmailpro: token 不能为空');
        }
        if ($address === '') {
            throw new \InvalidArgumentException('tempmailpro: 邮箱地址不能为空');
        }

        $resp = HttpClient::get(self::API_BASE . '/mailbox/' . rawurlencode($mailboxToken) . '/emails', self::HEADERS);
        $data = HttpClient::json($resp);
        $rows = $data['data'] ?? null;
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $messageId = trim((string) ($item['id'] ?? ''));
            $detail = $messageId !== '' ? self::fetchDetail($mailboxToken, $messageId) : null;
            $out[] = Normalize::email(self::flatten($detail ?? $item, $address), $address);
        }
        return $out;
    }

    /**
     * @return array<string,mixed>|null
     */
    private static function fetchDetail(string $token, string $messageId): ?array
    {
        $resp = HttpClient::get(
            self::API_BASE . '/mailbox/' . rawurlencode($token) . '/emails/' . rawurlencode($messageId),
            self::HEADERS,
        );
        if ($resp->getStatusCode() >= 400) {
            return null;
        }
        $data = HttpClient::json($resp);
        $detail = $data['data'] ?? null;
        return is_array($detail) ? $detail : null;
    }

    /**
     * @param array<mixed> $raw
     * @return array<string,mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        return [
            'id' => ($raw['id'] ?? '') ?: ($raw['message_id'] ?? ''),
            'from' => ($raw['from_address'] ?? '') ?: ($raw['from_name'] ?? ''),
            'to' => $recipient,
            'subject' => $raw['subject'] ?? '',
            'text' => $raw['body_text'] ?? '',
            'html' => $raw['body_html'] ?? '',
            'date' => $raw['received_at'] ?? '',
            'attachments' => $raw['attachments'] ?? [],
        ];
    }
}
