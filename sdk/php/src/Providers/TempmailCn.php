<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use ChanhanzhanX\TempMail\SocketIo;

/**
 * tempmail-cn 渠道实现（tempmail.cn）
 *
 * generate: Socket.IO 连接 → request mailbox → 收到 mailbox 事件
 * getEmails: REST API GET https://{host}/api/mails/{email}
 */
final class TempmailCn
{
    private const CHANNEL = 'tempmail-cn';
    private const DEFAULT_HOST = 'tempmail.cn';

    private const HEADERS = [
        'User-Agent'      => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
        'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
        'Cache-Control'   => 'no-cache',
        'DNT'             => '1',
        'Pragma'          => 'no-cache',
    ];

    /**
     * 通过 Socket.IO 请求邮箱地址。
     */
    public static function generate(?string $domain = null): EmailInfo
    {
        $host = self::normalizeHost($domain);
        $sio = SocketIo::connect($host);
        try {
            $sio->emit('request mailbox', true);
            $mailbox = $sio->waitForEvent('mailbox', 15000);
            if (!is_string($mailbox) || trim($mailbox) === '') {
                throw new \RuntimeException('tempmail-cn: mailbox 为空');
            }
            $email = trim($mailbox) . '@' . $host;
            return new EmailInfo(self::CHANNEL, $email);
        } finally {
            $sio->close();
        }
    }

    /**
     * 通过 REST API 获取邮件列表。
     *
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $trimmed = trim($email);
        $atPos = strpos($trimmed, '@');
        if ($atPos === false || $atPos === 0) {
            throw new \InvalidArgumentException('tempmail-cn: invalid email address');
        }
        $host = substr($trimmed, $atPos + 1) ?: self::DEFAULT_HOST;

        $url = "https://{$host}/api/mails/" . rawurlencode($trimmed);
        $resp = HttpClient::get($url, self::HEADERS + [
            'Accept'  => 'application/json',
            'Referer' => "https://{$host}/",
        ]);

        if ($resp->getStatusCode() >= 400) {
            return [];
        }

        $data = HttpClient::json($resp);
        $mails = $data['mails'] ?? [];
        if (!is_array($mails)) {
            return [];
        }

        $result = [];
        foreach ($mails as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $flat = self::flattenMail($raw, $trimmed);
            $em = Normalize::email($flat, $trimmed);
            if (!empty($em->id)) {
                $result[] = $em;
            }
        }
        return $result;
    }

    /**
     * 展平邮件原始数据。
     *
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flattenMail(array $raw, string $recipientEmail): array
    {
        $headers = $raw['headers'] ?? [];
        if (!is_array($headers)) {
            $headers = [];
        }

        $msgId = (string) ($raw['id'] ?? $raw['messageId'] ?? $headers['message-id'] ?? $headers['messageId'] ?? '');
        if ($msgId === '') {
            $msgId = implode("\n", [
                (string) ($headers['from'] ?? ''),
                (string) ($headers['subject'] ?? ''),
                (string) ($headers['date'] ?? ''),
                $recipientEmail,
            ]);
        }

        return [
            'id'      => $msgId,
            'from'    => (string) ($headers['from'] ?? $raw['from'] ?? ''),
            'to'      => $recipientEmail,
            'subject' => (string) ($headers['subject'] ?? $raw['subject'] ?? ''),
            'text'    => (string) ($raw['text'] ?? ''),
            'html'    => (string) ($raw['html'] ?? ''),
            'date'    => (string) ($headers['date'] ?? $raw['date'] ?? ''),
            'isRead'  => false,
            'attachments' => $raw['attachments'] ?? [],
        ];
    }

    private static function normalizeHost(?string $domain): string
    {
        $raw = trim($domain ?? '');
        if ($raw === '') {
            return self::DEFAULT_HOST;
        }
        $host = $raw;
        $lower = strtolower($host);
        if (str_starts_with($lower, 'http://') || str_starts_with($lower, 'https://')) {
            $host = explode('://', $host, 2)[1];
        }
        if (str_contains($host, '@')) {
            $parts = explode('@', $host);
            $host = end($parts);
        }
        if (str_contains($host, '/')) {
            $host = explode('/', $host, 2)[0];
        }
        $host = trim($host, '.');
        return $host ?: self::DEFAULT_HOST;
    }
}
