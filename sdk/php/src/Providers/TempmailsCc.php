<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * tempmails.cc 渠道实现
 *
 * https://tempmails.cc —— 地址即邮箱：在公开免费域名上生成地址，无需注册。
 * 详情正文位于 textBody / htmlBody。
 */
final class TempmailsCc
{
    private const API_BASE = 'https://tempmails.cc';

    /** @var array<string,string> */
    private const HEADERS = ['Accept' => 'application/json'];

    /**
     * @return array<string,mixed>
     */
    private static function getJson(string $url): array
    {
        $resp = HttpClient::get($url, self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('tempmails-cc: 请求失败');
        }
        return HttpClient::json($resp);
    }

    private static function pickDomain(): string
    {
        $data = self::getJson(self::API_BASE . '/api/domains');
        foreach (($data['domainInfo'] ?? []) as $item) {
            $domain = trim((string) ($item['domain'] ?? ''));
            if ($domain !== '') {
                return $domain;
            }
        }
        throw new \RuntimeException('tempmails-cc: 无可用公开域名');
    }

    /**
     * 在公开免费域名上生成地址。
     */
    public static function generate(string $channel = 'tempmails-cc'): EmailInfo
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $local = '';
        for ($i = 0; $i < 16; $i++) {
            $local .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return new EmailInfo($channel, $local . '@' . self::pickDomain());
    }

    /**
     * 跳过选域，在指定固定域名上生成地址。
     */
    public static function generateAtDomain(string $channel, string $domain): EmailInfo
    {
        $selected = trim($domain);
        if ($selected === '') {
            throw new \RuntimeException('tempmails-cc: 域名为空');
        }
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $local = '';
        for ($i = 0; $i < 16; $i++) {
            $local .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return new EmailInfo($channel, $local . '@' . $selected);
    }

    /**
     * 获取 tempmails.cc 邮件，详情正文位于 textBody / htmlBody。
     *
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('tempmails-cc: 邮箱地址为空');
        }
        $encoded = str_replace('%40', '@', rawurlencode($address));
        $rows = self::getJson(self::API_BASE . '/api/inbox/' . $encoded)['messages'] ?? null;
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
            try {
                $detail = self::getJson(
                    self::API_BASE . '/api/message/' . rawurlencode($messageId)
                    . '?address=' . $encoded . '&mark_read=false',
                );
                $to = $detail['to'] ?? $address;
                if (is_array($to)) {
                    $to = $to ? $to[array_key_first($to)] : $address;
                }
                $out[] = Normalize::email([
                    'id' => $detail['id'] ?? $messageId,
                    'from' => $detail['from'] ?? '',
                    'to' => $to ?: $address,
                    'subject' => $detail['subject'] ?? '',
                    'text' => $detail['textBody'] ?? '',
                    'html' => $detail['htmlBody'] ?? '',
                    'date' => $detail['date'] ?? '',
                    'seen' => $detail['isRead'] ?? false,
                ], $address);
                continue;
            } catch (\RuntimeException) {
                // 详情失败时退回摘要
            }
            $out[] = Normalize::email([
                'id' => $messageId,
                'from' => $item['from'] ?? '',
                'to' => $address,
                'subject' => $item['subject'] ?? '',
                'date' => $item['date'] ?? '',
                'seen' => $item['isRead'] ?? false,
            ], $address);
        }
        return $out;
    }
}
