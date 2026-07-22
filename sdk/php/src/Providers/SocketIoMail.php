<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\Normalize;
use ChanhanzhanX\TempMail\SocketIo;

/**
 * Socket.IO 临时邮箱共享实现
 *
 * 用于 mjj.cm、linshi.co 等使用相同 Socket.IO 协议的站点。
 * generate: 连接 → request shortid → 接收 shortid → 断开
 * getEmails: 连接 → set shortid → 阻塞监听 mail 事件 → 收集后断开
 */
final class SocketIoMail
{
    /** @var int 收信阻塞等待时间（毫秒） */
    private const GET_EMAILS_WAIT_MS = 5000;

    /**
     * 请求 shortid 并生成邮箱。
     */
    public static function generate(string $channel, string $host): EmailInfo
    {
        $sio = SocketIo::connect($host);
        try {
            $sio->emit('request shortid', true);
            $shortid = $sio->waitForEvent('shortid', 15000);
            if (!is_string($shortid) || trim($shortid) === '') {
                throw new \RuntimeException("{$channel}: shortid 为空");
            }
            $email = trim($shortid) . '@' . $host;
            return new EmailInfo($channel, $email);
        } finally {
            $sio->close();
        }
    }

    /**
     * 阻塞式收信：连接后订阅 shortid，等待 mail 事件。
     *
     * @return Email[]
     */
    public static function getEmails(string $channel, string $email, ?string $token): array
    {
        $trimmed = trim($email);
        $atPos = strpos($trimmed, '@');
        if ($atPos === false || $atPos === 0) {
            throw new \InvalidArgumentException("{$channel}: invalid email address");
        }
        $local = substr($trimmed, 0, $atPos);
        $host = substr($trimmed, $atPos + 1) ?: $channel;

        $sio = SocketIo::connect($host);
        try {
            $sio->emit('set shortid', $local);

            $result = [];
            $deadline = microtime(true) + (self::GET_EMAILS_WAIT_MS / 1000.0);

            while (true) {
                $remaining = (int) (($deadline - microtime(true)) * 1000);
                if ($remaining <= 0) {
                    break;
                }
                $event = $sio->receiveEvent($remaining);
                if ($event === null) {
                    break;
                }
                if ($event[0] === 'mail' && is_array($event[1])) {
                    $flat = self::flattenMail($event[1], $trimmed);
                    $result[] = Normalize::email($flat, $trimmed);
                }
            }
            return $result;
        } finally {
            $sio->close();
        }
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
                (string) ($headers['from'] ?? $raw['from'] ?? ''),
                (string) ($headers['subject'] ?? $raw['subject'] ?? ''),
                (string) ($headers['date'] ?? $raw['date'] ?? ''),
                $recipientEmail,
            ]);
        }

        return [
            'id'      => $msgId,
            'from'    => (string) ($headers['from'] ?? $raw['from'] ?? ''),
            'to'      => $recipientEmail,
            'subject' => (string) ($headers['subject'] ?? $raw['subject'] ?? ''),
            'text'    => (string) ($raw['text'] ?? $raw['body'] ?? ''),
            'html'    => (string) ($raw['html'] ?? ''),
            'date'    => (string) ($headers['date'] ?? $raw['date'] ?? ''),
            'isRead'  => false,
        ];
    }
}
