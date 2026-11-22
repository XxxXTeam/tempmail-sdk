<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Tempmailify 渠道实现
 *
 * https://tempmailify.com —— 创建邮箱返回收件箱专属 Bearer 令牌；详情提供完整 text/html。
 */
final class Tempmailify
{
    private const API_BASE = 'https://tempmailify.com';

    /**
     * @return array<string,string>
     */
    private static function bearerHeaders(string $bearer): array
    {
        return [
            'Accept' => 'application/json',
            'Authorization' => 'Bearer ' . $bearer,
        ];
    }

    private static function fromAddress(mixed $value): string
    {
        if (is_array($value)) {
            return (string) ($value['address'] ?? '');
        }
        return is_string($value) ? $value : '';
    }

    /**
     * @return array<string,mixed>
     */
    private static function getJson(string $url, string $bearer): array
    {
        $resp = HttpClient::get($url, self::bearerHeaders($bearer));
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('tempmailify: 请求失败');
        }
        return HttpClient::json($resp);
    }

    /**
     * 创建 Tempmailify 临时邮箱。
     */
    public static function generate(string $channel = 'tempmailify'): EmailInfo
    {
        $resp = HttpClient::post(
            self::API_BASE . '/api/inbox',
            ['Accept' => 'application/json', 'Content-Type' => 'application/json'],
            null,
            '{}',
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('tempmailify: 创建邮箱失败');
        }
        $data = HttpClient::json($resp);
        $address = trim((string) ($data['address'] ?? ''));
        $token = trim((string) ($data['token'] ?? ''));
        if ($address === '' || $token === '') {
            throw new \RuntimeException('tempmailify: 创建响应不完整');
        }
        return new EmailInfo($channel, $address, $token, null, $data['createdAt'] ?? null);
    }

    /**
     * 获取 Tempmailify 邮件，详情提供完整 text/html。
     *
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $address = trim($email);
        $bearer = trim((string) $token);
        if ($address === '' || $bearer === '') {
            throw new \InvalidArgumentException('tempmailify: 邮箱地址或令牌为空');
        }
        $base = self::API_BASE . '/api/inbox/' . str_replace('%40', '@', rawurlencode($address));
        $rows = self::getJson($base, $bearer)['messages'] ?? null;
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $messageId = trim((string) ($item['id'] ?? ''));
            if ($messageId === '') {
                continue;
            }
            try {
                $detail = self::getJson($base . '/messages/' . rawurlencode($messageId), $bearer);
                $out[] = Normalize::email([
                    'id' => $detail['id'] ?? $messageId,
                    'from' => self::fromAddress($detail['from'] ?? null),
                    'to' => $detail['to'] ?? $address,
                    'subject' => ($detail['subject'] ?? '') !== '' ? $detail['subject'] : ($item['subject'] ?? ''),
                    'text' => $detail['text'] ?? '',
                    'html' => $detail['html'] ?? '',
                    'date' => $detail['receivedAt'] ?? ($item['receivedAt'] ?? ''),
                    'seen' => $detail['seen'] ?? false,
                    'attachments' => $detail['attachments'] ?? [],
                ], $address);
                continue;
            } catch (\RuntimeException) {
                // 详情失败时退回摘要
            }
            $out[] = Normalize::email([
                'id' => $messageId,
                'from' => self::fromAddress($item['from'] ?? null),
                'to' => $address,
                'subject' => $item['subject'] ?? '',
                'text' => $item['preview'] ?? '',
                'date' => $item['receivedAt'] ?? '',
                'seen' => $item['seen'] ?? false,
            ], $address);
        }
        return $out;
    }
}
