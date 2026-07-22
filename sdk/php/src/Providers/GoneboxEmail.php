<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * gonebox-email 渠道实现 — https://gonebox.email
 *
 * 一次性临时邮箱，无需认证。
 *   创建: POST /api/v1/inboxes  body {"domain":"gonebox.email"}
 *   读信: GET  /api/v1/inboxes/{address}/messages
 */
final class GoneboxEmail
{
    private const CHANNEL = 'gonebox-email';
    private const BASE_URL = 'https://api.gonebox.email/api/v1';
    private const DOMAIN = 'gonebox.email';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36',
        'Accept' => 'application/json',
        'Content-Type' => 'application/json',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(
            self::BASE_URL . '/inboxes',
            self::HEADERS,
            json: ['domain' => self::DOMAIN],
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('gonebox-email: 创建邮箱失败 ' . $resp->getStatusCode());
        }
        $body = HttpClient::json($resp);
        if (empty($body['success'])) {
            throw new \RuntimeException('gonebox-email: 创建邮箱失败');
        }
        $data = $body['data'] ?? null;
        if (!is_array($data)) {
            throw new \RuntimeException('gonebox-email: 响应 data 格式无效');
        }
        $address = (string) ($data['address'] ?? '');
        if ($address === '') {
            throw new \RuntimeException('gonebox-email: 缺少邮箱地址');
        }
        // expiresAt 为秒级时间戳，转为毫秒
        $expiresAt = null;
        if (!empty($data['expiresAt']) && is_numeric($data['expiresAt'])) {
            $expiresAt = (int) $data['expiresAt'] * 1000;
        }
        return new EmailInfo(self::CHANNEL, $address, '', expiresAt: $expiresAt);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('gonebox-email: 缺少邮箱地址');
        }
        $headers = self::HEADERS;
        unset($headers['Content-Type']);

        $resp = HttpClient::get(self::BASE_URL . '/inboxes/' . $address . '/messages', $headers);
        if ($resp->getStatusCode() === 404) {
            return [];
        }
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('gonebox-email: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $body = HttpClient::json($resp);
        if (empty($body['success'])) {
            return [];
        }
        $data = $body['data'] ?? null;
        if (!is_array($data)) {
            return [];
        }
        $messages = $data['messages'] ?? null;
        if (!is_array($messages) || $messages === []) {
            return [];
        }
        $out = [];
        foreach ($messages as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $out[] = Normalize::email([
                'id' => $msg['id'] ?? '',
                'from' => $msg['from'] ?? ($msg['sender'] ?? ''),
                'to' => $address,
                'subject' => $msg['subject'] ?? '',
                'text' => $msg['text'] ?? ($msg['body_plain'] ?? ''),
                'html' => $msg['html'] ?? ($msg['body_html'] ?? ''),
                'date' => (string) ($msg['date'] ?? ($msg['received_at'] ?? '')),
                'is_read' => false,
                'attachments' => [],
            ], $address);
        }
        return $out;
    }
}
