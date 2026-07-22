<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * CleanTempMail 渠道实现 — https://cleantempmail.com
 *
 * 需 X-API-Key（默认公共 ct-test，可用环境变量 CLEANTEMPMAIL_API_KEY 覆盖）。
 * 创建 GET /api/generate-email，读信 GET /api/emails?email=xxx，取 data.emails。
 */
final class CleanTempMail
{
    private const CHANNEL = 'cleantempmail';
    private const API_BASE = 'https://cleantempmail.com/api';

    private static function apiKey(): string
    {
        $key = trim((string) getenv('CLEANTEMPMAIL_API_KEY'));
        return $key !== '' ? $key : 'ct-test';
    }

    /**
     * @return array<string,string>
     */
    private static function headers(): array
    {
        return [
            'Accept' => 'application/json',
            'User-Agent' => 'Mozilla/5.0',
            'X-API-Key' => self::apiKey(),
        ];
    }

    /**
     * @return array<mixed>
     */
    private static function getJson(string $path): array
    {
        $resp = HttpClient::get(self::API_BASE . $path, self::headers());
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('cleantempmail: 请求失败 ' . $resp->getStatusCode());
        }
        return HttpClient::json($resp);
    }

    public static function generate(): EmailInfo
    {
        $data = self::getJson('/generate-email');
        $payload = is_array($data['data'] ?? null) ? $data['data'] : [];
        $email = trim((string) ($payload['email'] ?? ($payload['mailbox'] ?? '')));
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('cleantempmail: invalid generate-email response');
        }
        return new EmailInfo(self::CHANNEL, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('cleantempmail: empty email');
        }
        $data = self::getJson('/emails?email=' . rawurlencode($address));
        $payload = is_array($data['data'] ?? null) ? $data['data'] : [];
        $rows = $payload['emails'] ?? null;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $item) {
            if (is_array($item)) {
                $out[] = Normalize::email($item, $address);
            }
        }
        return $out;
    }
}
