<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Catchmail 渠道实现（含 mailistry / zeppost 别名）
 *
 * https://catchmail.io，无需认证的 REST API。
 * 支持三个域名：catchmail.io / mailistry.com / zeppost.com。
 */
final class Catchmail
{
    private const API_BASE = 'https://api.catchmail.io/api/v1';
    private const DEFAULT_DOMAIN = 'catchmail.io';

    /** @var string[] */
    private const DOMAINS = ['catchmail.io', 'mailistry.com', 'zeppost.com'];

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'Referer' => 'https://catchmail.io/',
        'Origin' => 'https://catchmail.io',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = 'sdk';
        for ($i = 0; $i < 16; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    private static function pickDomain(?string $preferred): string
    {
        $p = strtolower(ltrim(trim((string) $preferred), '@'));
        if ($p !== '') {
            foreach (self::DOMAINS as $d) {
                if (strtolower($d) === $p) {
                    return $d;
                }
            }
        }
        return self::DEFAULT_DOMAIN;
    }

    /**
     * 清洗地址：数组取首个，剥离 "Name <addr>" 中的尖括号内容。
     */
    private static function cleanAddress(mixed $value): string
    {
        if (is_array($value)) {
            return empty($value) ? '' : self::cleanAddress($value[array_key_first($value)]);
        }
        $text = trim((string) ($value ?? ''));
        if (preg_match('/<([^>]+)>/', $text, $m)) {
            return trim($m[1]);
        }
        return $text;
    }

    public static function generate(?string $domain = null, string $channel = 'catchmail'): EmailInfo
    {
        $selectedChannel = trim($channel) ?: 'catchmail';
        $email = self::randomLocal() . '@' . self::pickDomain($domain);
        $resp = HttpClient::get(self::API_BASE . '/mailbox?address=' . rawurlencode($email), self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('catchmail: 创建邮箱失败');
        }
        return new EmailInfo($selectedChannel, $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('catchmail: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::API_BASE . '/mailbox?address=' . rawurlencode($address), self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('catchmail: 拉取邮件失败');
        }
        $data = HttpClient::json($resp);
        $rows = $data['messages'] ?? null;
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
            $detail = self::fetchMessage($messageId, $address);
            if ($detail !== null) {
                $body = is_array($detail['body'] ?? null) ? $detail['body'] : [];
                $out[] = Normalize::email([
                    'id' => $detail['id'] ?? '',
                    'from' => self::cleanAddress($detail['from'] ?? null),
                    'to' => self::cleanAddress($detail['to'] ?? null) ?: $address,
                    'subject' => $detail['subject'] ?? '',
                    'text' => $body['text'] ?? '',
                    'html' => $body['html'] ?? '',
                    'date' => $detail['date'] ?? '',
                    'attachments' => $detail['attachments'] ?? null,
                ], $address);
                continue;
            }
            $out[] = Normalize::email([
                'id' => $messageId,
                'from' => self::cleanAddress($item['from'] ?? null),
                'to' => $item['mailbox'] ?? $address,
                'subject' => $item['subject'] ?? '',
                'date' => $item['date'] ?? '',
            ], $address);
        }
        return $out;
    }

    /**
     * @return array<mixed>|null
     */
    private static function fetchMessage(string $messageId, string $email): ?array
    {
        try {
            $resp = HttpClient::get(
                self::API_BASE . '/message/' . rawurlencode($messageId) . '?mailbox=' . rawurlencode($email),
                self::HEADERS,
            );
            if ($resp->getStatusCode() >= 400) {
                return null;
            }
            $data = HttpClient::json($resp);
            return !empty($data) ? $data : null;
        } catch (\Throwable) {
            return null;
        }
    }
}
