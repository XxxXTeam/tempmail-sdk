<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Byom 渠道实现 — https://byom.de
 *
 * 纯 GET 无认证：本地生成随机用户名拼固定域名，读信按用户名查询。
 */
final class Byom
{
    private const CHANNEL = 'byom';
    private const DOMAIN = 'byom.de';
    private const BASE = 'https://api.byom.de';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json, text/plain, */*',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    private static function randomUsername(int $length = 10): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * 生成随机邮箱地址，无需 API 调用。
     */
    public static function generate(): EmailInfo
    {
        $email = self::randomUsername() . '@' . self::DOMAIN;
        return new EmailInfo(self::CHANNEL, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $e = trim($email);
        if ($e === '') {
            throw new \InvalidArgumentException('byom: empty email');
        }
        $username = explode('@', $e)[0];
        if ($username === '') {
            throw new \InvalidArgumentException('byom: invalid email format');
        }
        $resp = HttpClient::get(self::BASE . '/mails/' . rawurlencode($username), self::HEADERS);
        $data = HttpClient::json($resp);
        if (!array_is_list($data)) {
            return [];
        }
        $out = [];
        foreach ($data as $raw) {
            if (is_array($raw)) {
                $out[] = Normalize::email($raw, $e);
            }
        }
        return $out;
    }
}
