<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * restmail-net 渠道实现 — https://restmail.net
 *
 * Mozilla 开源项目，无需创建邮箱（ad-hoc 模式）：随机生成 username，
 * 邮箱即 username@restmail.net，收信 GET /mail/{username}。
 */
final class RestmailNet
{
    private const CHANNEL = 'restmail-net';
    private const BASE = 'https://restmail.net';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36',
    ];

    private static function randomUsername(): string
    {
        $length = random_int(10, 12);
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(): EmailInfo
    {
        $email = self::randomUsername() . '@restmail.net';
        return new EmailInfo(self::CHANNEL, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('restmail-net: 邮箱地址为空');
        }
        $username = explode('@', $addr)[0];
        if ($username === '') {
            throw new \RuntimeException('restmail-net: 无法提取用户名');
        }
        $resp = HttpClient::get(self::BASE . '/mail/' . $username, self::HEADERS);
        if ($resp->getStatusCode() === 404) {
            return [];
        }
        $data = HttpClient::json($resp);
        $out = [];
        foreach ($data as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $fromAddr = '';
            $fromList = $msg['from'] ?? null;
            if (is_array($fromList) && isset($fromList[0]) && is_array($fromList[0])) {
                $fromAddr = (string) ($fromList[0]['address'] ?? '');
            }
            if ($fromAddr === '') {
                $headers = $msg['headers'] ?? null;
                if (is_array($headers)) {
                    $fromAddr = (string) ($headers['from'] ?? '');
                }
            }
            $row = [
                'id' => $msg['id'] ?? '',
                'from' => $fromAddr,
                'to' => $addr,
                'subject' => $msg['subject'] ?? '',
                'text' => (string) ($msg['text'] ?? ''),
                'html' => (string) ($msg['html'] ?? ''),
                'date' => $msg['receivedAt'] ?? '',
                'is_read' => false,
                'attachments' => [],
            ];
            $out[] = Normalize::email($row, $addr);
        }
        return $out;
    }
}
