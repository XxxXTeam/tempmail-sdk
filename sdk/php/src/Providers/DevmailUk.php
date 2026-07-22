<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * DevMail UK 渠道实现 — https://devmail.uk
 *
 * 创建 GET /api/new 取邮箱地址；读信 GET /api/inbox/{mailbox}?detail=true，取 emails 数组。
 */
final class DevmailUk
{
    private const CHANNEL = 'devmail-uk';
    private const API_BASE = 'https://www.devmail.uk/api';

    /** @var array<string,string> */
    private const HEADERS = ['Accept' => 'application/json'];

    private static function mailboxFromEmail(string $email): string
    {
        $address = trim($email);
        if ($address === '') {
            return '';
        }
        if (str_contains($address, '@')) {
            return trim(explode('@', $address, 2)[0]);
        }
        return $address;
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::API_BASE . '/new', self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('devmail-uk: 创建邮箱失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['email'] ?? ''));
        if ($email === '' && trim((string) ($data['mailbox'] ?? '')) !== '') {
            $email = trim((string) $data['mailbox']) . '@devmail.uk';
        }
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('devmail-uk: invalid new email response');
        }
        return new EmailInfo(self::CHANNEL, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $mailbox = self::mailboxFromEmail($email);
        if ($mailbox === '') {
            throw new \InvalidArgumentException('devmail-uk: empty email');
        }
        $resp = HttpClient::get(
            self::API_BASE . '/inbox/' . rawurlencode($mailbox) . '?detail=true',
            self::HEADERS,
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('devmail-uk: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $rows = $data['emails'] ?? null;
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $item) {
            if (is_array($item)) {
                $out[] = Normalize::email($item, $email);
            }
        }
        return $out;
    }
}
