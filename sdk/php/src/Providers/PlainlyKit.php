<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * PlainlyKit 渠道实现
 *
 * https://plainlykit.com —— 创建邮箱返回内部令牌；内部令牌格式为 “邮箱ID|读取令牌”。
 */
final class PlainlyKit
{
    private const API_BASE = 'https://plainlykit.com/tools/temporary-email/api';

    /**
     * @return array{0:string,1:string}
     */
    private static function splitToken(?string $token): array
    {
        $parts = explode('|', trim((string) $token), 2);
        if (count($parts) !== 2 || $parts[0] === '' || $parts[1] === '') {
            throw new \InvalidArgumentException('plainlykit: 内部令牌格式错误');
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
            throw new \RuntimeException('plainlykit: 请求失败');
        }
        return HttpClient::json($resp);
    }

    /**
     * 创建 PlainlyKit 临时邮箱。
     */
    public static function generate(string $channel = 'plainlykit'): EmailInfo
    {
        $resp = HttpClient::post(
            self::API_BASE . '/mailboxes',
            ['Accept' => 'application/json', 'Content-Type' => 'application/json'],
            null,
            '{}',
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('plainlykit: 创建邮箱失败');
        }
        $data = HttpClient::json($resp);
        $address = trim((string) ($data['address'] ?? ''));
        $id = trim((string) ($data['id'] ?? ''));
        $token = trim((string) ($data['token'] ?? ''));
        if ($address === '' || $id === '' || $token === '') {
            throw new \RuntimeException('plainlykit: 创建响应不完整');
        }
        return new EmailInfo($channel, $address, $id . '|' . $token, null, $data['createdAt'] ?? null);
    }

    /**
     * 获取 PlainlyKit 邮件，详情提供纯文本正文。
     *
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        [$id, $bearer] = self::splitToken($token);
        $base = self::API_BASE . '/mailboxes/' . rawurlencode($id) . '/messages';
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
                $detail = self::getJson($base . '/' . rawurlencode($messageId), $bearer);
                $raw = is_array($detail['message'] ?? null) ? $detail['message'] : $detail;
                $out[] = Normalize::email([
                    'id' => $raw['id'] ?? $messageId,
                    'from' => ($raw['sender'] ?? '') !== '' ? $raw['sender'] : ($item['sender'] ?? ''),
                    'to' => $email,
                    'subject' => ($raw['subject'] ?? '') !== '' ? $raw['subject'] : ($item['subject'] ?? ''),
                    'text' => $raw['text'] ?? '',
                    'html' => $raw['html'] ?? '',
                    'date' => $raw['receivedAt'] ?? '',
                ], $email);
                continue;
            } catch (\RuntimeException) {
                // 详情失败时退回摘要
            }
            $out[] = Normalize::email([
                'id' => $messageId,
                'from' => $item['sender'] ?? '',
                'to' => $email,
                'subject' => $item['subject'] ?? '',
                'text' => $item['preview'] ?? '',
                'date' => $item['receivedAt'] ?? '',
            ], $email);
        }
        return $out;
    }
}
