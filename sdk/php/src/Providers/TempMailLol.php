<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * tempmail.lol 渠道实现
 *
 * API: https://api.tempmail.lol/v2
 */
final class TempMailLol
{
    private const BASE_URL = 'https://api.tempmail.lol/v2';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36',
        'Content-Type' => 'application/json',
        'Origin' => 'https://tempmail.lol',
        'DNT' => '1',
    ];

    public static function generate(?string $domain = null): EmailInfo
    {
        $resp = HttpClient::post(
            self::BASE_URL . '/inbox/create',
            self::HEADERS,
            json: ['domain' => $domain, 'captcha' => null],
        );
        $data = HttpClient::json($resp);
        if (empty($data['address']) || empty($data['token'])) {
            throw new \RuntimeException('tempmail-lol: 创建邮箱失败');
        }
        return new EmailInfo('tempmail-lol', (string) $data['address'], (string) $data['token']);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('tempmail-lol: token 不能为空');
        }
        $resp = HttpClient::get(self::BASE_URL . '/inbox?token=' . rawurlencode($token), self::HEADERS);
        $data = HttpClient::json($resp);
        $out = [];
        foreach (($data['emails'] ?? []) as $raw) {
            if (is_array($raw)) {
                $out[] = Normalize::email($raw, $email);
            }
        }
        return $out;
    }
}
