<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * temp-mail.org 渠道实现
 *
 * API: https://web2.temp-mail.org
 * 流程: POST /mailbox 创建邮箱（返回 JWT token + mailbox）→ GET /messages 列表 → GET /messages/{id} 详情。
 * 认证: Authorization: Bearer {token}。
 */
final class TempMailOrg
{
    private const CHANNEL = 'temp-mail-org';
    private const BASE_URL = 'https://web2.temp-mail.org';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
        'Origin' => 'https://temp-mail.org',
        'Referer' => 'https://temp-mail.org/',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::BASE_URL . '/mailbox', self::HEADERS);
        $data = HttpClient::json($resp);
        $token = (string) ($data['token'] ?? '');
        $mailbox = (string) ($data['mailbox'] ?? '');
        if ($token === '' || $mailbox === '') {
            throw new \RuntimeException('temp-mail-org: 创建邮箱失败');
        }
        return new EmailInfo(self::CHANNEL, $mailbox, $token);
    }

    /**
     * @return array<string,string>
     */
    private static function authHeaders(string $token): array
    {
        return self::HEADERS + ['Authorization' => 'Bearer ' . $token];
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('temp-mail-org: token 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('temp-mail-org: 邮箱地址不能为空');
        }

        $resp = HttpClient::get(self::BASE_URL . '/messages', self::authHeaders($token));
        $data = HttpClient::json($resp);
        $messages = $data['messages'] ?? null;
        if (!is_array($messages) || empty($messages)) {
            return [];
        }

        $out = [];
        foreach ($messages as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $msgId = (string) ($msg['_id'] ?? '');
            if ($msgId === '') {
                continue;
            }
            $detail = self::fetchDetail($token, $msgId);
            $raw = [
                'id' => $msgId,
                'from' => $detail['from'] ?? ($msg['from'] ?? ''),
                'to' => $addr,
                'subject' => $detail['subject'] ?? ($msg['subject'] ?? ''),
                'text' => $detail['bodyPreview'] ?? ($msg['bodyPreview'] ?? ''),
                'html' => $detail['bodyHtml'] ?? '',
                'date' => $detail['createdAt'] ?? (string) ($msg['receivedAt'] ?? ''),
            ];
            $out[] = Normalize::email($raw, $addr);
        }
        return $out;
    }

    /**
     * @return array<string,mixed>
     */
    private static function fetchDetail(string $token, string $msgId): array
    {
        try {
            $resp = HttpClient::get(self::BASE_URL . '/messages/' . $msgId, self::authHeaders($token));
            if ($resp->getStatusCode() >= 400) {
                return [];
            }
            return HttpClient::json($resp);
        } catch (\Throwable) {
            return [];
        }
    }
}
