<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * temp-mail.io 渠道实现
 *
 * API: https://api.internal.temp-mail.io/api/v3
 */
final class TempMailIo
{
    private const BASE_URL = 'https://api.internal.temp-mail.io/api/v3';
    private const PAGE_URL = 'https://temp-mail.io/en';

    private static ?string $cachedCorsHeader = null;

    private static function corsHeader(): string
    {
        if (self::$cachedCorsHeader !== null) {
            return self::$cachedCorsHeader;
        }
        try {
            $resp = HttpClient::get(self::PAGE_URL, [
                'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36',
            ]);
            $html = (string) $resp->getBody();
            if (preg_match('/mobileTestingHeader\s*:\s*"([^"]+)"/', $html, $m)) {
                self::$cachedCorsHeader = $m[1];
                return self::$cachedCorsHeader;
            }
        } catch (\Throwable) {
        }
        self::$cachedCorsHeader = '1';
        return self::$cachedCorsHeader;
    }

    /**
     * @return array<string,string>
     */
    private static function headers(): array
    {
        return [
            'Content-Type' => 'application/json',
            'Application-Name' => 'web',
            'Application-Version' => '4.0.0',
            'X-CORS-Header' => self::corsHeader(),
            'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/144.0.0.0',
            'Origin' => 'https://temp-mail.io',
            'Referer' => 'https://temp-mail.io/',
        ];
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(
            self::BASE_URL . '/email/new',
            self::headers(),
            json: ['min_name_length' => 10, 'max_name_length' => 10],
        );
        $data = HttpClient::json($resp);
        $email = (string) ($data['email'] ?? '');
        $token = (string) ($data['token'] ?? '');
        if ($email === '' || $token === '') {
            throw new \RuntimeException('temp-mail-io: 无效的创建响应');
        }
        return new EmailInfo('temp-mail-io', $email, $token);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $resp = HttpClient::get(self::BASE_URL . '/email/' . rawurlencode($email) . '/messages', self::headers());
        $data = HttpClient::json($resp);
        $out = [];
        foreach ($data as $raw) {
            if (is_array($raw)) {
                $out[] = Normalize::email($raw, $email);
            }
        }
        return $out;
    }
}
