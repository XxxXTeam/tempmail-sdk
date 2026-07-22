<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * mailcat-ai 渠道实现 — https://mailcat.ai
 *
 * POST /mailboxes 创建邮箱，响应 data.token 为 Bearer 令牌；
 * GET /inbox 携带 Authorization: Bearer 拉取邮件。
 */
final class MailcatAi
{
    private const BASE_URL = 'https://api.mailcat.ai';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36',
        'Accept' => 'application/json',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::BASE_URL . '/mailboxes', self::HEADERS);
        $body = HttpClient::json($resp);
        $data = $body['data'] ?? null;
        if (!is_array($data)) {
            throw new \RuntimeException('mailcat-ai: 响应 data 格式无效');
        }
        $address = (string) ($data['email'] ?? '');
        $token = (string) ($data['token'] ?? '');
        if ($address === '') {
            throw new \RuntimeException('mailcat-ai: 缺少邮箱地址');
        }
        if ($token === '') {
            throw new \RuntimeException('mailcat-ai: 缺少认证令牌');
        }
        return new EmailInfo('mailcat-ai', $address, $token);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('mailcat-ai: token 不能为空');
        }
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('mailcat-ai: 缺少邮箱地址');
        }

        $resp = HttpClient::get(
            self::BASE_URL . '/inbox',
            self::HEADERS + ['Authorization' => 'Bearer ' . $token],
        );
        if ($resp->getStatusCode() === 404) {
            return [];
        }
        $body = HttpClient::json($resp);
        $messages = $body['data'] ?? null;
        if (!is_array($messages)) {
            return [];
        }

        $out = [];
        foreach ($messages as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $raw = [
                'id' => $msg['id'] ?? '',
                'from' => $msg['from'] ?? ($msg['sender'] ?? ''),
                'to' => $address,
                'subject' => $msg['subject'] ?? '',
                'text' => $msg['text'] ?? ($msg['body_plain'] ?? ''),
                'html' => $msg['html'] ?? ($msg['body_html'] ?? ''),
                'date' => (string) ($msg['date'] ?? ($msg['received_at'] ?? '')),
            ];
            $out[] = Normalize::email($raw, $address);
        }
        return $out;
    }
}
