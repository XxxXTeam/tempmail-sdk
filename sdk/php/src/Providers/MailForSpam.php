<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * MailForSpam 渠道实现（含 tempmail.io / disposable.email 别名）
 *
 * https://mailforspam.com，无需认证的 REST API。
 */
final class MailForSpam
{
    private const API_BASE = 'https://api.mailforspam.com/api';
    private const DEFAULT_DOMAIN = 'mailforspam.com';

    /** @var string[] */
    private const DOMAINS = ['mailforspam.com', 'tempmail.io', 'disposable.email'];

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'Referer' => 'https://mailforspam.com/',
        'Origin' => 'https://mailforspam.com',
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

    private static function mailboxUrl(string $email, int $pageSize): string
    {
        return self::API_BASE . '/mailboxes/' . rawurlencode($email) . '/emails'
            . '?page=1&page_size=' . $pageSize . '&sort_by=date&sort_order=desc';
    }

    public static function generate(?string $domain = null, string $channel = 'mailforspam'): EmailInfo
    {
        $selectedChannel = trim($channel) ?: 'mailforspam';
        $email = self::randomLocal() . '@' . self::pickDomain($domain);
        $resp = HttpClient::get(self::mailboxUrl($email, 1), self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('mailforspam: 创建邮箱失败');
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
            throw new \InvalidArgumentException('mailforspam: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::mailboxUrl($address, 100), self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('mailforspam: 拉取邮件失败');
        }
        $data = HttpClient::json($resp);
        $rows = $data['emails'] ?? null;
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
            $raw = $detail ?? $item;
            $out[] = Normalize::email([
                'id' => $raw['id'] ?? '',
                'from' => $raw['sender'] ?? '',
                'to' => $raw['receiver'] ?? $address,
                'subject' => $raw['subject'] ?? '',
                'text' => $raw['body_text'] ?? '',
                'html' => $raw['body_html'] ?? '',
                'date' => $raw['date'] ?? '',
                'isRead' => ($raw['readAt'] ?? null) !== null,
                'attachments' => $raw['attachments'] ?? null,
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
                self::API_BASE . '/mailboxes/' . rawurlencode($email) . '/emails/' . rawurlencode($messageId),
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
