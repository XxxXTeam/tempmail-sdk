<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * mail.cx 渠道实现
 *
 * 匿名 Web API: https://mail.cx/v1，使用随机 X-Client-ID 作为凭据。
 */
final class MailCx
{
    private const BASE_URL = 'https://mail.cx';

    private static function randomString(int $length): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * @return array<string,string>
     */
    private static function headers(string $clientId): array
    {
        return [
            'Accept' => 'application/json',
            'Origin' => self::BASE_URL,
            'Referer' => self::BASE_URL . '/',
            'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
            'X-Client-ID' => $clientId,
        ];
    }

    public static function generate(?string $preferredDomain = null, string $channel = 'mail-cx'): EmailInfo
    {
        $selectedChannel = trim($channel) ?: 'mail-cx';
        $clientId = 'tempmail-sdk-' . self::randomString(16);
        $resp = HttpClient::get(self::BASE_URL . '/v1/config', self::headers($clientId));
        $config = HttpClient::json($resp);

        $rawDomains = $config['system_domains'] ?? [];
        $domains = [];
        $defaultDomain = '';
        foreach ((is_array($rawDomains) ? $rawDomains : []) as $item) {
            if (is_array($item) && !empty($item['domain'])) {
                $d = (string) $item['domain'];
                $domains[] = $d;
                if (!empty($item['default']) && $defaultDomain === '') {
                    $defaultDomain = $d;
                }
            }
        }
        if (empty($domains)) {
            throw new \RuntimeException('mail-cx: 无可用域名');
        }

        // 域名优先级：显式偏好 > default 标记 > 首个
        $domain = $defaultDomain !== '' ? $defaultDomain : $domains[0];
        $preferred = strtolower(ltrim((string) ($preferredDomain ?? ''), '@'));
        if ($preferred !== '') {
            foreach ($domains as $d) {
                if (strtolower($d) === $preferred) {
                    $domain = $d;
                    break;
                }
            }
        }

        $expiresAt = null;
        $ttl = $config['ttl_seconds'] ?? null;
        if (is_numeric($ttl) && $ttl > 0) {
            $expiresAt = (int) ((time() + (int) $ttl) * 1000);
        }

        return new EmailInfo(
            $selectedChannel,
            self::randomString(12) . '@' . $domain,
            $clientId,
            expiresAt: $expiresAt,
            createdAt: date('c'),
        );
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $clientId): array
    {
        if ($clientId === null || $clientId === '') {
            throw new \InvalidArgumentException('mail-cx: client id 不能为空');
        }
        $resp = HttpClient::get(self::BASE_URL . '/v1/inbox/' . rawurlencode($email), self::headers($clientId));
        if ($resp->getStatusCode() === 204) {
            return [];
        }
        $data = HttpClient::json($resp);
        $rows = $data['emails'] ?? [];
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                $out[] = Normalize::email([], $email);
                continue;
            }
            $raw = [
                'id' => $row['id'] ?? '',
                'from' => self::firstNonEmpty($row['from_email'] ?? null, $row['from_name'] ?? null),
                'to' => $email,
                'subject' => $row['subject'] ?? '',
                'text' => $row['preview_text'] ?? '',
                'created_at' => $row['created_at'] ?? null,
                'attachments' => $row['attachments'] ?? null,
            ];
            $messageId = (string) ($row['id'] ?? '');
            if ($messageId !== '') {
                try {
                    $dr = HttpClient::get(self::BASE_URL . '/v1/email/' . rawurlencode($messageId), self::headers($clientId));
                    if ($dr->getStatusCode() < 400) {
                        $detail = HttpClient::json($dr);
                        if (!empty($detail)) {
                            $raw = [
                                'id' => $detail['id'] ?? '',
                                'from' => self::firstNonEmpty($detail['from_email'] ?? null, $detail['from_name'] ?? null),
                                'to' => $email,
                                'subject' => $detail['subject'] ?? '',
                                'text_body' => $detail['text_body'] ?? '',
                                'html_body' => $detail['html_body'] ?? '',
                                'created_at' => $detail['created_at'] ?? null,
                                'attachments' => $detail['attachments'] ?? null,
                            ];
                        }
                    }
                } catch (\Throwable) {
                }
            }
            $out[] = Normalize::email($raw, $email);
        }
        return $out;
    }

    private static function firstNonEmpty(mixed ...$values): string
    {
        foreach ($values as $v) {
            if ($v === null) {
                continue;
            }
            $t = trim((string) $v);
            if ($t !== '') {
                return $t;
            }
        }
        return '';
    }
}
