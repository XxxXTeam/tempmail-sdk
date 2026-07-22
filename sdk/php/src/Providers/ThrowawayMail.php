<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * throwawaymail.app 渠道实现
 *
 * API: https://throwawaymail.app/api
 * 流程: POST /mailboxes 创建邮箱（mailbox_id 作为 token）→
 *       GET /mailboxes/{id}/messages 列表 → GET /mailboxes/{id}/messages/{mid} 详情。
 */
final class ThrowawayMail
{
    private const CHANNEL = 'throwawaymail';
    private const API_BASE = 'https://throwawaymail.app/api';

    /** @var array<string,string> */
    private const HEADERS = ['Accept' => 'application/json'];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::API_BASE . '/mailboxes', self::HEADERS);
        $data = HttpClient::json($resp);
        $mailboxId = trim((string) ($data['mailbox_id'] ?? ''));
        $email = trim((string) ($data['address'] ?? ''));
        if ($mailboxId === '' || $email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('throwawaymail: 创建邮箱响应无效');
        }
        $exp = $data['expires_at'] ?? null;
        return new EmailInfo(
            self::CHANNEL,
            $email,
            $mailboxId,
            expiresAt: is_numeric($exp) ? (int) $exp : null,
            createdAt: $data['created_at'] ?? null,
        );
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $mailboxId = trim((string) $token);
        $address = trim($email);
        if ($mailboxId === '') {
            throw new \InvalidArgumentException('throwawaymail: mailbox id 不能为空');
        }
        if ($address === '') {
            throw new \InvalidArgumentException('throwawaymail: 邮箱地址不能为空');
        }

        $resp = HttpClient::get(self::API_BASE . '/mailboxes/' . rawurlencode($mailboxId) . '/messages', self::HEADERS);
        $rows = HttpClient::json($resp);
        if (!array_is_list($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $messageId = trim((string) ($item['message_id'] ?? ''));
            $detail = $messageId !== '' ? self::fetchMessage($mailboxId, $messageId) : null;
            $out[] = Normalize::email(self::flatten($detail ?? $item, $address), $address);
        }
        return $out;
    }

    /**
     * @return array<string,mixed>|null
     */
    private static function fetchMessage(string $mailboxId, string $messageId): ?array
    {
        $resp = HttpClient::get(
            self::API_BASE . '/mailboxes/' . rawurlencode($mailboxId) . '/messages/' . rawurlencode($messageId),
            self::HEADERS,
        );
        if ($resp->getStatusCode() >= 400) {
            return null;
        }
        $data = HttpClient::json($resp);
        return !empty($data) ? $data : null;
    }

    /**
     * @param array<mixed> $raw
     * @return array<string,mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        $toAddresses = $raw['to_addresses'] ?? null;
        $to = (is_array($toAddresses) && isset($toAddresses[0])) ? (string) $toAddresses[0] : $recipient;
        return [
            'id' => $raw['message_id'] ?? '',
            'messageId' => $raw['message_id'] ?? '',
            'from_address' => $raw['from_address'] ?? '',
            'fromName' => $raw['from_name'] ?? '',
            'to' => $to,
            'subject' => $raw['subject'] ?? '',
            'received_at' => $raw['received_at'] ?? '',
            'read' => $raw['read'] ?? false,
            'text' => $raw['text'] ?? '',
            'html' => $raw['html'] ?? '',
            'size' => $raw['size'] ?? null,
        ];
    }
}
