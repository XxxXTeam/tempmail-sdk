<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * TempGBox 渠道实现（https://tempgbox.net）
 *
 * API 经 /api/proxy 中转，响应体为 HTML 内含 base64 编码的 JSON（data-x 属性）。
 * generate POST ?route=generate（variant=googlemail），token = 随机 X-Device-ID；
 * getEmails POST ?route=inbox 拉取 messages。
 */
final class Tempgbox
{
    private const CHANNEL = 'tempgbox';
    private const API_URL = 'https://tempgbox.net/api/proxy';
    private const ORIGIN = 'https://tempgbox.net';

    /** @var array<string,string> */
    private const DEFAULT_HEADERS = [
        'Accept' => 'text/html,application/json',
        'Content-Type' => 'application/json',
        'Origin' => self::ORIGIN,
        'Referer' => self::ORIGIN . '/',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
    ];

    private static function randomDeviceId(): string
    {
        return bin2hex(random_bytes(32));
    }

    private static function randomIp(): string
    {
        return implode('.', [
            random_int(1, 254),
            random_int(1, 254),
            random_int(1, 254),
            random_int(1, 254),
        ]);
    }

    /**
     * 从响应 HTML 中提取 data-x base64 载荷并解码为 JSON。
     *
     * @return array<mixed>
     */
    private static function decodePayload(string $html): array
    {
        $marker = 'data-x="';
        $quote = '"';
        $start = strpos($html, $marker);
        if ($start === false) {
            $marker = "data-x='";
            $quote = "'";
            $start = strpos($html, $marker);
        }
        if ($start === false) {
            throw new \RuntimeException('tempgbox: missing encoded response payload');
        }
        $start += strlen($marker);
        $end = strpos($html, $quote, $start);
        if ($end === false) {
            throw new \RuntimeException('tempgbox: malformed encoded response payload');
        }
        $raw = base64_decode(substr($html, $start, $end - $start), true);
        if ($raw === false) {
            throw new \RuntimeException('tempgbox: invalid base64 payload');
        }
        $data = json_decode($raw, true);
        return is_array($data) ? $data : [];
    }

    /**
     * @param array<mixed> $payload
     * @return array<mixed>
     */
    private static function postProxy(string $route, string $deviceId, array $payload): array
    {
        $ip = self::randomIp();
        $headers = self::DEFAULT_HEADERS;
        $headers['X-Device-ID'] = $deviceId;
        $headers['X-Forwarded-For'] = $ip;
        $headers['X-Real-IP'] = $ip;
        $headers['X-Originating-IP'] = $ip;
        $resp = HttpClient::post(self::API_URL . '?route=' . $route, $headers, $payload);
        $data = self::decodePayload((string) $resp->getBody());
        if ($resp->getStatusCode() >= 400) {
            $reason = $data['detail'] ?? $data['error'] ?? $data['message'] ?? $resp->getReasonPhrase();
            throw new \RuntimeException("tempgbox {$route} failed: " . $resp->getStatusCode() . ' ' . $reason);
        }
        return $data;
    }

    public static function generate(): EmailInfo
    {
        $deviceId = self::randomDeviceId();
        $data = self::postProxy('generate', $deviceId, ['variant' => 'googlemail']);
        $alias = $data['alias'] ?? [];
        if (!is_array($alias)) {
            $alias = [];
        }
        $email = (string) ($alias['email'] ?? $alias['alias'] ?? '');
        if ($email === '') {
            throw new \RuntimeException('tempgbox: missing email');
        }
        $expiresAt = $alias['expires_at'] ?? null;
        return new EmailInfo(
            self::CHANNEL,
            $email,
            $deviceId,
            is_numeric($expiresAt) ? (int) $expiresAt : null,
            $alias['created_at'] ?? null,
        );
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \RuntimeException('tempgbox: missing device id');
        }
        $data = self::postProxy('inbox', $token, ['email' => $email]);
        $messages = $data['messages'] ?? [];
        if (!is_array($messages)) {
            $messages = [];
        }
        $out = [];
        foreach ($messages as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $flat = $raw;
            $flat['from'] = $raw['from'] ?? $raw['sender'] ?? '';
            $flat['text'] = $raw['text'] ?? $raw['body_text'] ?? '';
            $flat['html'] = $raw['html'] ?? $raw['body_html'] ?? '';
            $flat['date'] = $raw['date'] ?? $raw['received_at'] ?? '';
            $flat['messageId'] = $raw['messageId'] ?? $raw['message_id'] ?? $raw['id'] ?? '';
            $flat['attachments'] = $raw['attachments'] ?? $raw['attachments_info'] ?? [];
            $out[] = Normalize::email($flat, $email);
        }
        return $out;
    }
}
