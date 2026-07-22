<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * ta-easy.com 渠道实现
 *
 * API: https://api-endpoint.ta-easy.com/temp-email/
 * 流程: POST /temp-email/address/new（空 body）创建邮箱 → POST /temp-email/inbox/list 读信。
 */
final class TaEasy
{
    private const CHANNEL = 'ta-easy';
    private const API_BASE = 'https://api-endpoint.ta-easy.com';
    private const ORIGIN = 'https://www.ta-easy.com';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
        'origin' => self::ORIGIN,
        'referer' => self::ORIGIN . '/',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(
            self::API_BASE . '/temp-email/address/new',
            self::HEADERS + ['Content-Length' => '0'],
            body: '',
        );
        $data = HttpClient::json($resp);
        if (($data['status'] ?? '') !== 'success' || empty($data['address']) || empty($data['token'])) {
            $msg = (string) ($data['message'] ?? 'create failed');
            throw new \RuntimeException('ta-easy: ' . $msg);
        }
        $exp = $data['expiresAt'] ?? null;
        $expiresAt = is_numeric($exp) ? (int) $exp : null;
        return new EmailInfo(self::CHANNEL, (string) $data['address'], (string) $data['token'], expiresAt: $expiresAt);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('ta-easy: token 不能为空');
        }
        $resp = HttpClient::post(
            self::API_BASE . '/temp-email/inbox/list',
            self::HEADERS + ['Content-Type' => 'application/json'],
            json: ['token' => $token, 'email' => $email],
        );
        $data = HttpClient::json($resp);
        if (($data['status'] ?? '') !== 'success') {
            $msg = (string) ($data['message'] ?? 'inbox failed');
            throw new \RuntimeException('ta-easy: ' . $msg);
        }
        $list = $data['data'] ?? [];
        if (!is_array($list)) {
            return [];
        }
        $out = [];
        foreach ($list as $raw) {
            if (is_array($raw)) {
                $out[] = Normalize::email($raw, $email);
            }
        }
        return $out;
    }
}
