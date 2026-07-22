<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * nimail 渠道实现 — https://www.nimail.cn
 *
 * 简单 POST 表单 API，无需认证，Token 即邮箱地址本身。
 * 1. POST /api/applymail (mail=前缀@nimail.cn) → 注册邮箱
 * 2. POST /api/getmails (mail=&time=0) → 返回 {"mail": [...], "success": "true"}
 */
final class Nimail
{
    private const CHANNEL = 'nimail';
    private const BASE = 'https://www.nimail.cn';
    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36';

    /**
     * @return array<string,string>
     */
    private static function formHeaders(): array
    {
        return [
            'User-Agent' => self::UA,
            'Content-Type' => 'application/x-www-form-urlencoded',
            'Origin' => self::BASE,
            'Referer' => self::BASE . '/',
        ];
    }

    private static function randomLocal(int $n = 10): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $n; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * 按优先级从消息中提取首个非空值并转为字符串。
     *
     * @param array<mixed> $msg
     */
    private static function first(array $msg, string ...$keys): string
    {
        foreach ($keys as $key) {
            $val = $msg[$key] ?? null;
            if ($val !== null && $val !== '') {
                return (string) $val;
            }
        }
        return '';
    }

    public static function generate(): EmailInfo
    {
        $email = self::randomLocal() . '@nimail.cn';
        $resp = HttpClient::post(
            self::BASE . '/api/applymail',
            self::formHeaders(),
            body: 'mail=' . rawurlencode($email),
        );
        $data = HttpClient::json($resp);
        if (($data['success'] ?? null) !== 'true' || empty($data['user'])) {
            throw new \RuntimeException('nimail: 创建邮箱失败');
        }
        $user = (string) $data['user'];
        return new EmailInfo(self::CHANNEL, $user, $user);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('nimail: 缺少邮箱地址');
        }
        $resp = HttpClient::post(
            self::BASE . '/api/getmails',
            self::formHeaders(),
            body: 'mail=' . rawurlencode($addr) . '&time=0',
        );
        $data = HttpClient::json($resp);
        if (($data['success'] ?? null) !== 'true') {
            return [];
        }
        $mail = $data['mail'] ?? null;
        if (!is_array($mail)) {
            return [];
        }
        $out = [];
        foreach ($mail as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $row = [
                'id' => self::first($msg, 'id', 'time'),
                'from' => self::first($msg, 'from', 'sender'),
                'to' => $addr,
                'subject' => self::first($msg, 'subject', 'title'),
                'text' => self::first($msg, 'text', 'content'),
                'html' => self::first($msg, 'html', 'content'),
                'date' => self::first($msg, 'date', 'time'),
            ];
            $out[] = Normalize::email($row, $addr);
        }
        return $out;
    }
}
