<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * mail.tm 渠道实现
 *
 * API: https://api.mail.tm
 * 流程: 获取域名 → 生成随机邮箱/密码 → 创建账号 → 获取 Bearer Token
 */
final class MailTm
{
    private const BASE_URL = 'https://api.mail.tm';

    /** @var array<string,string> */
    private const HEADERS = [
        'Content-Type' => 'application/json',
        'Accept' => 'application/json',
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
     * 获取可用域名列表，兼容 Hydra 格式与纯数组。
     *
     * @return string[]
     */
    private static function getDomains(): array
    {
        $resp = HttpClient::get(self::BASE_URL . '/domains', self::HEADERS);
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

    public static function generate(string $channel = 'mail-tm'): EmailInfo
    {
        $selectedChannel = trim($channel) ?: 'mail-tm';
        $domains = self::getDomains();
        if (empty($domains)) {
            throw new \RuntimeException('mail-tm: 无可用域名');
        }
        $domain = $domains[array_rand($domains)];
        $address = self::randomString(12) . '@' . $domain;
        $password = self::randomString(16);

        $accResp = HttpClient::post(
            self::BASE_URL . '/accounts',
            ['Content-Type' => 'application/ld+json', 'Accept' => 'application/json'],
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
            throw new \RuntimeException('mail-tm: 获取 token 失败');
        }

        return new EmailInfo($selectedChannel, $address, $token, createdAt: $account['createdAt'] ?? null);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('mail-tm: token 不能为空');
        }
        $auth = self::HEADERS + ['Authorization' => 'Bearer ' . $token];
        $resp = HttpClient::get(self::BASE_URL . '/messages', $auth);
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
                    // 忽略详情获取失败，回退列表数据
                }
            }
            $result[] = Normalize::email(self::flatten($flat, $email), $email);
        }
        return $result;
    }

    /**
     * 将 mail.tm 消息格式扁平化。
     *
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
