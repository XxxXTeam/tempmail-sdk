<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * tempgo.email 渠道实现
 *
 * API: https://tempgo.email
 * 流程: POST /api/generate（无 body，不发 Content-Type）→ 返回 mailbox_id 作为 token；
 *       GET /api/inbox?email={email}&mailbox_id={token} 读信。
 */
final class TempgoEmail
{
    private const CHANNEL = 'tempgo-email';
    private const BASE_URL = 'https://tempgo.email';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36',
        'Accept' => 'application/json',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::BASE_URL . '/api/generate', self::HEADERS);
        $body = HttpClient::json($resp);
        $address = (string) ($body['email'] ?? '');
        $mailboxId = (string) ($body['mailbox_id'] ?? '');
        if ($address === '') {
            throw new \RuntimeException('tempgo-email: 缺少邮箱地址');
        }
        if ($mailboxId === '') {
            throw new \RuntimeException('tempgo-email: 缺少 mailbox_id');
        }
        return new EmailInfo(self::CHANNEL, $address, $mailboxId);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('tempgo-email: token 不能为空');
        }
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('tempgo-email: 缺少邮箱地址');
        }

        $resp = HttpClient::get(
            self::BASE_URL . '/api/inbox',
            self::HEADERS,
            query: ['email' => $address, 'mailbox_id' => $token],
        );
        if ($resp->getStatusCode() === 404) {
            return [];
        }
        $body = HttpClient::json($resp);
        $messages = $body['messages'] ?? null;
        if (!is_array($messages) || empty($messages)) {
            return [];
        }

        $out = [];
        foreach ($messages as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $raw = [
                'id' => $msg['id'] ?? '',
                'from' => ($msg['from'] ?? '') ?: ($msg['sender'] ?? ''),
                'to' => $address,
                'subject' => $msg['subject'] ?? '',
                'text' => ($msg['text'] ?? '') ?: ($msg['body_plain'] ?? ''),
                'html' => ($msg['html'] ?? '') ?: ($msg['body_html'] ?? ''),
                'date' => (string) (($msg['date'] ?? '') ?: ($msg['received_at'] ?? '')),
            ];
            $out[] = Normalize::email($raw, $address);
        }
        return $out;
    }
}
