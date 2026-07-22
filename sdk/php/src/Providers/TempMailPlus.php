<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * TempMail Plus 渠道实现
 *
 * https://tempmail.plus，无需认证的简单 REST API，默认域名 mailto.plus。
 */
final class TempMailPlus
{
    private const API_BASE = 'https://tempmail.plus/api/mails';
    private const DOMAIN = 'mailto.plus';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
        'Referer' => 'https://tempmail.plus/',
        'Origin' => 'https://tempmail.plus',
    ];

    private static function randomLocal(int $length = 12): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(?string $domain = null, string $channel = 'tempmail-plus'): EmailInfo
    {
        $selectedDomain = trim((string) ($domain ?? self::DOMAIN)) ?: self::DOMAIN;
        $selectedChannel = trim($channel) ?: 'tempmail-plus';
        $email = self::randomLocal() . '@' . $selectedDomain;
        // 调用列表接口验证地址可用
        HttpClient::get(self::API_BASE . '/?email=' . rawurlencode($email) . '&epin=', self::HEADERS);
        return new EmailInfo($selectedChannel, $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $e = trim($email);
        if ($e === '') {
            throw new \InvalidArgumentException('tempmail-plus: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::API_BASE . '/?email=' . rawurlencode($e) . '&epin=', self::HEADERS);
        $data = HttpClient::json($resp);
        $rows = $data['mail_list'] ?? null;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $mailId = $raw['mail_id'] ?? 0;
            $detail = self::fetchBody($mailId, $e);
            $out[] = Normalize::email([
                'id' => $mailId ? (string) $mailId : '',
                'from' => $raw['from_mail'] ?? $raw['from_name'] ?? '',
                'to' => $e,
                'subject' => $raw['subject'] ?? '',
                'date' => $raw['time'] ?? '',
                'html' => $detail['html'] ?? '',
                'text' => $detail['text'] ?? '',
                'isRead' => !($raw['is_new'] ?? true),
            ], $e);
        }
        return $out;
    }

    /**
     * @return array<mixed>
     */
    private static function fetchBody(mixed $mailId, string $email): array
    {
        if (!$mailId) {
            return [];
        }
        try {
            $resp = HttpClient::get(self::API_BASE . '/' . rawurlencode((string) $mailId) . '?email=' . rawurlencode($email) . '&epin=', self::HEADERS);
            $data = HttpClient::json($resp);
            return is_array($data) ? $data : [];
        } catch (\Throwable) {
            return [];
        }
    }
}
