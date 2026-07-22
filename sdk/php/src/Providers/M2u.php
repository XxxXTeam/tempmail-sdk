<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * MailToYou / m2u 渠道实现 — https://m2u.io
 *
 * 创建 POST /v1/mailboxes/auto → {mailbox:{local_part,domain,token,view_token}}；
 * token 与 view_token 打包为 JSON 存入 EmailInfo.token。
 * 读信 GET /v1/mailboxes/{token}/messages?view={view_token}，逐封补全正文。
 */
final class M2u
{
    private const CHANNEL = 'm2u';
    private const API_BASE = 'https://api.m2u.io';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
    ];

    private static function packToken(string $token, string $viewToken): string
    {
        return (string) json_encode(['token' => $token, 'viewToken' => $viewToken]);
    }

    /**
     * @return array{0:string,1:string}
     */
    private static function unpackToken(?string $token): array
    {
        $value = trim((string) $token);
        if ($value === '') {
            return ['', ''];
        }
        if (str_starts_with($value, '{')) {
            $data = json_decode($value, true);
            if (is_array($data)) {
                return [
                    trim((string) ($data['token'] ?? '')),
                    trim((string) ($data['viewToken'] ?? '')),
                ];
            }
            return ['', ''];
        }
        return [$value, ''];
    }

    /**
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        return [
            'id' => $raw['id'] ?? ($raw['message_id'] ?? ''),
            'from' => $raw['from_addr'] ?? ($raw['from_address'] ?? ($raw['from'] ?? '')),
            'to' => $recipient,
            'subject' => $raw['subject'] ?? '',
            'text' => $raw['text_body'] ?? ($raw['body_text'] ?? ($raw['text'] ?? '')),
            'html' => $raw['html_body'] ?? ($raw['body_html'] ?? ($raw['html'] ?? '')),
            'date' => $raw['received_at'] ?? ($raw['created_at'] ?? ''),
            'is_read' => $raw['is_read'] ?? ($raw['isRead'] ?? ($raw['seen'] ?? false)),
            'attachments' => $raw['attachments'] ?? [],
        ];
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(
            self::API_BASE . '/v1/mailboxes/auto',
            self::HEADERS + ['Content-Type' => 'application/json'],
            json: [],
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('m2u: 创建邮箱失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $mailbox = $data['mailbox'] ?? null;
        if (!is_array($mailbox)) {
            throw new \RuntimeException('m2u: invalid mailbox response');
        }
        $localPart = trim((string) ($mailbox['local_part'] ?? ''));
        $domain = trim((string) ($mailbox['domain'] ?? ''));
        $token = trim((string) ($mailbox['token'] ?? ''));
        $viewToken = trim((string) ($mailbox['view_token'] ?? ''));
        $email = ($localPart !== '' && $domain !== '') ? $localPart . '@' . $domain : '';
        if ($email === '' || $token === '' || $viewToken === '') {
            throw new \RuntimeException('m2u: invalid mailbox response');
        }
        $expiresAt = null;
        if (isset($mailbox['expires_at']) && is_numeric($mailbox['expires_at'])) {
            $expiresAt = (int) $mailbox['expires_at'];
        }
        return new EmailInfo(
            self::CHANNEL,
            $email,
            self::packToken($token, $viewToken),
            expiresAt: $expiresAt,
            createdAt: $mailbox['created_at'] ?? null,
        );
    }

    /**
     * 获取单封邮件详情，失败返回 null。
     *
     * @return array<mixed>|null
     */
    private static function fetchDetail(string $token, string $viewToken, string $messageId): ?array
    {
        $resp = HttpClient::get(
            self::API_BASE . '/v1/mailboxes/' . rawurlencode($token) . '/messages/' . rawurlencode($messageId),
            self::HEADERS,
            query: ['view' => $viewToken],
        );
        if ($resp->getStatusCode() >= 400) {
            return null;
        }
        $data = HttpClient::json($resp);
        return is_array($data['message'] ?? null) ? $data['message'] : null;
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        [$mailboxToken, $viewToken] = self::unpackToken($token);
        $address = trim($email);
        if ($mailboxToken === '') {
            throw new \InvalidArgumentException('m2u: missing token');
        }
        if ($viewToken === '') {
            throw new \InvalidArgumentException('m2u: missing view token');
        }
        if ($address === '') {
            throw new \InvalidArgumentException('m2u: empty email');
        }
        $resp = HttpClient::get(
            self::API_BASE . '/v1/mailboxes/' . rawurlencode($mailboxToken) . '/messages',
            self::HEADERS,
            query: ['view' => $viewToken],
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('m2u: 收件箱请求失败 ' . $resp->getStatusCode());
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
            $messageId = trim((string) ($item['id'] ?? ($item['message_id'] ?? '')));
            $detail = $messageId !== '' ? self::fetchDetail($mailboxToken, $viewToken, $messageId) : null;
            $out[] = Normalize::email(self::flatten($detail ?? $item, $address), $address);
        }
        return $out;
    }
}
