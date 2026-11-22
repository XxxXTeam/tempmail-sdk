<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Nextmail 渠道实现
 *
 * https://nextmail.sh —— 创建收件箱返回内部令牌；内部令牌格式为 “收件箱ID|读取令牌”。
 * 详情正文位于 message.decodedBody（HTML）。
 */
final class Nextmail
{
    private const API_BASE = 'https://api.nextmail.sh';

    /**
     * @return array{0:string,1:string}
     */
    private static function splitToken(?string $token): array
    {
        $parts = explode('|', trim((string) $token), 2);
        if (count($parts) !== 2 || $parts[0] === '' || $parts[1] === '') {
            throw new \InvalidArgumentException('nextmail: 内部令牌格式错误');
        }
        return [$parts[0], $parts[1]];
    }

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

    /**
     * @return array<string,mixed>
     */
    private static function getJson(string $url, string $bearer): array
    {
        $resp = HttpClient::get($url, self::bearerHeaders($bearer));
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('nextmail: 请求失败');
        }
        return HttpClient::json($resp);
    }

    private static function fromEmail(mixed $value): string
    {
        if (is_array($value)) {
            return (string) ($value['email'] ?? '');
        }
        return is_string($value) ? $value : '';
    }

    /**
     * 租用 Nextmail 临时收件箱。
     */
    public static function generate(string $channel = 'nextmail'): EmailInfo
    {
        $resp = HttpClient::post(
            self::API_BASE . '/temp-mail/inboxes',
            ['Accept' => 'application/json', 'Content-Type' => 'application/json'],
            null,
            '{}',
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('nextmail: 创建邮箱失败');
        }
        $data = HttpClient::json($resp);
        $address = trim((string) ($data['address'] ?? ''));
        $id = trim((string) ($data['id'] ?? ''));
        $token = trim((string) ($data['token'] ?? ''));
        if ($address === '' || $id === '' || $token === '') {
            throw new \RuntimeException('nextmail: 创建响应不完整');
        }
        return new EmailInfo($channel, $address, $id . '|' . $token, null, $data['createdAt'] ?? null);
    }

    /**
     * 获取 Nextmail 邮件，详情正文位于 message.decodedBody。
     *
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        [$id, $bearer] = self::splitToken($token);
        $base = self::API_BASE . '/temp-mail/inboxes/' . rawurlencode($id) . '/messages';
        $rows = self::getJson($base . '?limit=50', $bearer)['data'] ?? null;
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
                $detail = self::getJson($base . '/' . rawurlencode($messageId), $bearer);
                $message = is_array($detail['message'] ?? null) ? $detail['message'] : [];
                $out[] = Normalize::email([
                    'id' => $detail['id'] ?? $messageId,
                    'from' => self::fromEmail($detail['from'] ?? null),
                    'to' => $detail['to'] ?? $email,
                    'subject' => $detail['subject'] ?? '',
                    'html' => $message['decodedBody'] ?? '',
                    'date' => $detail['receivedAt'] ?? '',
                    'seen' => $detail['seen'] ?? false,
                ], $email);
                continue;
            } catch (\RuntimeException) {
                // 详情失败时退回摘要
            }
            $out[] = Normalize::email([
                'id' => $messageId,
                'from' => self::fromEmail($item['from'] ?? null),
                'to' => $item['to'] ?? $email,
                'subject' => $item['subject'] ?? '',
                'text' => $item['snippet'] ?? '',
                'date' => $item['receivedAt'] ?? '',
                'seen' => $item['seen'] ?? false,
            ], $email);
        }
        return $out;
    }
}
