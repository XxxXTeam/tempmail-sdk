<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * tempmail.lol V2 渠道实现
 *
 * API: https://api.tempmail.lol
 * 流程: GET /generate 创建邮箱（address + token）→ GET /auth/{token} 读信。
 */
final class TempMailLolV2
{
    private const CHANNEL = 'tempmail-lol-v2';
    private const API_BASE = 'https://api.tempmail.lol';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::API_BASE . '/generate', self::HEADERS);
        $data = HttpClient::json($resp);
        if (empty($data['address']) || empty($data['token'])) {
            throw new \RuntimeException('tempmail-lol-v2: 缺少 address 或 token');
        }
        return new EmailInfo(self::CHANNEL, (string) $data['address'], (string) $data['token']);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('tempmail-lol-v2: token 不能为空');
        }
        $resp = HttpClient::get(self::API_BASE . '/auth/' . rawurlencode($token), self::HEADERS);
        $data = HttpClient::json($resp);
        $list = $data['email'] ?? null;
        if (!is_array($list)) {
            return [];
        }

        $out = [];
        foreach ($list as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $out[] = Normalize::email([
                'id' => ($raw['id'] ?? '') ?: ($raw['_id'] ?? ''),
                'from' => ($raw['from'] ?? '') ?: ($raw['sender'] ?? ''),
                'to' => $email,
                'subject' => $raw['subject'] ?? '',
                'text' => ($raw['body'] ?? '') ?: ($raw['text'] ?? ''),
                'html' => ($raw['html'] ?? '') ?: ($raw['body'] ?? ''),
                'date' => ($raw['date'] ?? '') ?: ($raw['receivedAt'] ?? ''),
                'isRead' => false,
            ], $email);
        }
        return $out;
    }
}
