<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * TempMailC 渠道实现 — https://tempmailc.com
 *
 * GET /new 创建邮箱；GET /inbox?email= 列表；GET /message?email=&msg_id= 详情。
 */
final class Tempmailc
{
    private const CHANNEL = 'tempmailc';
    private const API_BASE = 'https://tempmailc.com/api/v1';

    /** @var array<string,string> */
    private const HEADERS = ['Accept' => 'application/json'];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::API_BASE . '/new', self::HEADERS);
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['email'] ?? ''));
        if (empty($data['ok']) || $email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('tempmailc: 无效的创建响应');
        }
        return new EmailInfo(self::CHANNEL, $email);
    }

    /**
     * 拉取单封邮件详情，失败返回 null。
     *
     * @return array<mixed>|null
     */
    private static function fetchMessage(string $email, string $messageId): ?array
    {
        $resp = HttpClient::get(self::API_BASE . '/message', self::HEADERS, query: [
            'email' => $email,
            'msg_id' => $messageId,
        ]);
        if ($resp->getStatusCode() >= 400) {
            return null;
        }
        $data = HttpClient::json($resp);
        if (empty($data) || ($data['ok'] ?? null) === false) {
            return null;
        }
        return $data;
    }

    /**
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flattenList(array $raw, string $recipient): array
    {
        return [
            'id' => $raw['id'] ?? '',
            'from' => $raw['from'] ?? '',
            'to' => $recipient,
            'subject' => $raw['subject'] ?? '',
            'timestamp' => $raw['ts'] ?? null,
            'read' => $raw['read'] ?? null,
        ];
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('tempmailc: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::API_BASE . '/inbox', self::HEADERS, query: ['email' => $addr]);
        $data = HttpClient::json($resp);
        if (empty($data['ok'])) {
            throw new \RuntimeException('tempmailc: inbox 响应失败');
        }
        $rows = $data['messages'] ?? [];
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $messageId = trim((string) ($item['id'] ?? ''));
            $detail = $messageId !== '' ? self::fetchMessage($addr, $messageId) : null;
            $out[] = Normalize::email($detail ?? self::flattenList($item, $addr), $addr);
        }
        return $out;
    }
}
