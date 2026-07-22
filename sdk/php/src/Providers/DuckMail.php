<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * DuckMail 渠道实现
 *
 * API: https://api.duckmail.sbs（与 mail.tm 同构：域名 → 建号 → Bearer Token）
 */
final class DuckMail
{
    private const BASE_URL = 'https://api.duckmail.sbs';

    /** @var array<string,string> */
    private const HEADERS = [
        'Content-Type' => 'application/json',
        'Accept' => 'application/json',
        'Origin' => 'https://duckmail.sbs',
        'Referer' => 'https://duckmail.sbs/',
    ];

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
     * @return string[]
     */
    private static function getDomains(): array
    {
        $resp = HttpClient::get(self::BASE_URL . '/domains?page=1', self::HEADERS);
        $data = HttpClient::json($resp);
        $members = array_is_list($data) ? $data : ($data['hydra:member'] ?? []);
        $domains = [];
        foreach ($members as $d) {
            if (is_array($d) && ($d['isActive'] ?? false) && !empty($d['domain'])) {
                $domains[] = (string) $d['domain'];
            }
        }
        return $domains;
    }

    public static function generate(): EmailInfo
    {
        $domains = self::getDomains();
        if (empty($domains)) {
            throw new \RuntimeException('duckmail: 无可用域名');
        }
        $domain = $domains[array_rand($domains)];
        $address = self::randomString(12) . '@' . $domain;
        $password = self::randomString(16);

        $accResp = HttpClient::post(
            self::BASE_URL . '/accounts',
            self::HEADERS + ['Content-Type' => 'application/ld+json'],
            json: ['address' => $address, 'password' => $password],
        );
        $account = HttpClient::json($accResp);

        $tokResp = HttpClient::post(
            self::BASE_URL . '/token',
            self::HEADERS,
            json: ['address' => $address, 'password' => $password],
        );
        $token = (string) (HttpClient::json($tokResp)['token'] ?? '');
        if ($token === '') {
            throw new \RuntimeException('duckmail: 获取 token 失败');
        }

        return new EmailInfo('duckmail', $address, $token, createdAt: $account['createdAt'] ?? null);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('duckmail: token 不能为空');
        }
        $auth = self::HEADERS + ['Authorization' => 'Bearer ' . $token];
        $resp = HttpClient::get(self::BASE_URL . '/messages?page=1', $auth);
        $data = HttpClient::json($resp);
        $messages = array_is_list($data) ? $data : ($data['hydra:member'] ?? []);

        $result = [];
        foreach ($messages as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $flat = $msg;
            if (!empty($msg['id'])) {
                try {
                    $detailResp = HttpClient::get(self::BASE_URL . '/messages/' . $msg['id'], $auth);
                    if ($detailResp->getStatusCode() < 400) {
                        $detail = HttpClient::json($detailResp);
                        if (!empty($detail)) {
                            $flat = $detail;
                        }
                    }
                } catch (\Throwable) {
                }
            }
            $result[] = Normalize::email(self::flatten($flat, $email), $email);
        }
        return $result;
    }

    /**
     * @param array<mixed> $msg
     * @return array<mixed>
     */
    private static function flatten(array $msg, string $recipient): array
    {
        $from = '';
        if (is_array($msg['from'] ?? null)) {
            $from = (string) ($msg['from']['address'] ?? '');
        } elseif (is_string($msg['from'] ?? null)) {
            $from = $msg['from'];
        }
        $to = $recipient;
        if (is_array($msg['to'] ?? null) && isset($msg['to'][0]) && is_array($msg['to'][0])) {
            $to = (string) ($msg['to'][0]['address'] ?? $recipient);
        }
        $html = $msg['html'] ?? '';
        if (is_array($html)) {
            $html = implode('', $html);
        }
        return [
            'id' => $msg['id'] ?? '',
            'from' => $from,
            'to' => $to,
            'subject' => $msg['subject'] ?? '',
            'text' => $msg['text'] ?? '',
            'html' => $html,
            'createdAt' => $msg['createdAt'] ?? '',
            'seen' => $msg['seen'] ?? false,
            'attachments' => $msg['attachments'] ?? [],
        ];
    }
}
