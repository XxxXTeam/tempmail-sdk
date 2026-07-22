<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Mail.TD 渠道实现 — https://mail.td
 *
 * 使用 SHA-256 工作量证明（PoW）创建账户：
 *   GET /domains 取域名 → 本地求解 PoW → POST /accounts 建号（可能要求提升难度重试）；
 *   GET /accounts/{id}/messages 携带 Bearer JWT 拉取邮件。
 * token 序列化保存 {jwt, id}。
 */
final class MailTd
{
    private const BASE_URL = 'https://api.mail.td/api';
    private const INITIAL_DIFFICULTY = 15;
    private const MAX_DIFFICULTY = 24;
    private const MAX_RETRIES = 5;

    /** @var array<string,string> */
    private const HEADERS = [
        'Content-Type' => 'application/json',
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    ];

    private static function randomName(int $length = 12): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    private static function randomPassword(int $length = 16): string
    {
        $chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * 计算 SHA-256 摘要（原始字节）二进制表示的前导零位数。
     */
    private static function leadingZeroBits(string $digest): int
    {
        $bits = 0;
        $len = strlen($digest);
        for ($i = 0; $i < $len; $i++) {
            $byte = ord($digest[$i]);
            if ($byte === 0) {
                $bits += 8;
                continue;
            }
            for ($j = 7; $j >= 0; $j--) {
                if (($byte >> $j) & 1) {
                    return $bits;
                }
                $bits++;
            }
            return $bits;
        }
        return $bits;
    }

    /**
     * 求解 PoW：input = address + timestamp + nonce，SHA-256 前导零 >= difficulty。
     */
    private static function solvePow(string $address, int $timestamp, int $difficulty): string
    {
        $prefix = strtolower(trim($address)) . $timestamp;
        $nonce = 0;
        while (true) {
            $digest = hash('sha256', $prefix . $nonce, true);
            if (self::leadingZeroBits($digest) >= $difficulty) {
                return (string) $nonce;
            }
            $nonce++;
        }
    }

    /**
     * @return string[]
     */
    private static function fetchDomains(): array
    {
        $resp = HttpClient::get(self::BASE_URL . '/domains', self::HEADERS);
        $data = HttpClient::json($resp);
        $domains = $data['domains'] ?? null;
        if (!is_array($domains)) {
            throw new \RuntimeException('mail-td: 未获取到域名列表');
        }
        $result = [];
        foreach ($domains as $item) {
            if (!is_array($item) || !empty($item['pro_only'])) {
                continue;
            }
            $domain = (string) ($item['domain'] ?? '');
            if ($domain !== '') {
                $result[] = $domain;
            }
        }
        if (empty($result)) {
            throw new \RuntimeException('mail-td: 无可用域名');
        }
        return $result;
    }

    public static function generate(): EmailInfo
    {
        $domains = self::fetchDomains();
        $address = self::randomName() . '@' . $domains[array_rand($domains)];
        $authKey = hash('sha256', self::randomPassword());
        $difficulty = self::INITIAL_DIFFICULTY;

        for ($attempt = 0; $attempt <= self::MAX_RETRIES; $attempt++) {
            if ($difficulty > self::MAX_DIFFICULTY) {
                throw new \RuntimeException('mail-td: PoW 难度超出上限');
            }
            $timestamp = time();
            $nonce = self::solvePow($address, $timestamp, $difficulty);

            $resp = HttpClient::post(
                self::BASE_URL . '/accounts',
                self::HEADERS,
                json: [
                    'address' => $address,
                    'auth_key' => $authKey,
                    'pow' => ['t' => $timestamp, 'n' => $nonce, 'd' => $difficulty],
                ],
            );
            $data = HttpClient::json($resp);

            if (($data['status'] ?? null) === 'retry') {
                $required = $data['required_difficulty'] ?? null;
                if (!is_int($required) || $required <= $difficulty) {
                    $difficulty++;
                } else {
                    $difficulty = $required;
                }
                continue;
            }

            $accountId = (string) ($data['id'] ?? '');
            $jwt = (string) ($data['token'] ?? '');
            $addr = (string) ($data['address'] ?? $address);
            if ($accountId === '' || $jwt === '' || !str_contains($addr, '@')) {
                throw new \RuntimeException('mail-td: 创建账户失败');
            }
            $token = json_encode(['jwt' => $jwt, 'id' => $accountId], JSON_UNESCAPED_SLASHES);
            return new EmailInfo('mail-td', $addr, (string) $token);
        }

        throw new \RuntimeException('mail-td: PoW 重试次数已用尽，创建账户失败');
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('mail-td: token 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('mail-td: 邮箱地址不能为空');
        }
        $info = json_decode($token, true);
        if (!is_array($info)) {
            throw new \InvalidArgumentException('mail-td: token 格式无效');
        }
        $jwt = (string) ($info['jwt'] ?? '');
        $accountId = (string) ($info['id'] ?? '');
        if ($jwt === '' || $accountId === '') {
            throw new \InvalidArgumentException('mail-td: token 缺少 jwt 或 id');
        }

        $resp = HttpClient::get(
            self::BASE_URL . '/accounts/' . rawurlencode($accountId) . '/messages',
            self::HEADERS + ['Authorization' => 'Bearer ' . $jwt],
            query: ['page' => 1],
        );
        $data = HttpClient::json($resp);
        $messages = $data['messages'] ?? null;
        if (!is_array($messages)) {
            return [];
        }

        $out = [];
        foreach ($messages as $item) {
            if (!is_array($item)) {
                continue;
            }
            $sender = $item['from'] ?? null;
            if (is_array($sender)) {
                $fromAddr = (string) ($sender['address'] ?? '');
            } else {
                $fromAddr = (string) $sender;
            }
            $raw = [
                'id' => $item['id'] ?? '',
                'from' => $fromAddr,
                'to' => $item['to'] ?? $addr,
                'subject' => $item['subject'] ?? '',
                'text' => $item['text'] ?? '',
                'html' => $item['html'] ?? '',
                'date' => $item['created_at'] ?? '',
            ];
            $out[] = Normalize::email($raw, $addr);
        }
        return $out;
    }
}
