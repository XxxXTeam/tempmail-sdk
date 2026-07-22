<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * MailholeDe 渠道实现 — https://mailhole.de
 *
 * 公共临时邮箱，无需认证。
 * 1. GET /api/random → HTML 中包含随机邮箱地址
 * 2. GET /json/{email} → JSON 数组获取邮件
 * Token 即邮箱地址本身。
 */
final class MailholeDe
{
    private const CHANNEL = 'mailhole-de';
    private const BASE = 'https://mailhole.de';

    /**
     * 按优先级从消息中提取首个非空字符串值。
     *
     * @param array<mixed> $msg
     */
    private static function firstStr(array $msg, string ...$keys): string
    {
        foreach ($keys as $key) {
            $val = $msg[$key] ?? null;
            if (is_string($val) && $val !== '') {
                return $val;
            }
        }
        return '';
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE . '/api/random', ['Accept' => 'text/html']);
        $html = (string) $resp->getBody();
        if (!preg_match('/([a-z0-9.]+@mailhole\.de)/', $html, $m)) {
            throw new \RuntimeException('mailhole-de: 无法从响应中解析邮箱地址');
        }
        $email = $m[1];
        return new EmailInfo(self::CHANNEL, $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('mailhole-de: 缺少邮箱地址');
        }
        $resp = HttpClient::get(self::BASE . '/json/' . rawurlencode($addr), ['Accept' => 'application/json']);
        $text = (string) $resp->getBody();
        if ($text === '' || $text === '[]') {
            return [];
        }
        $messages = HttpClient::json($resp);
        $out = [];
        foreach ($messages as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $row = [
                'id' => $msg['id'] ?? '',
                'from' => self::firstStr($msg, 'sender', 'from'),
                'to' => $addr,
                'subject' => $msg['subject'] ?? '',
                'text' => self::firstStr($msg, 'body', 'text'),
                'html' => self::firstStr($msg, 'html', 'body'),
                'received_at' => self::firstStr($msg, 'date', 'received'),
            ];
            $out[] = Normalize::email($row, $addr);
        }
        return $out;
    }
}
