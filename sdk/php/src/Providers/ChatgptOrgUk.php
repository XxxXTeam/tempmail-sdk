<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * mail.chatgpt.org.uk 渠道实现
 *
 * API: https://mail.chatgpt.org.uk/api
 * 流程: 拉取公开域名 → 本地生成随机邮箱 → POST /inbox-token 拿 JWT + gm_sid → 拉邮件。
 */
final class ChatgptOrgUk
{
    private const BASE_URL = 'https://mail.chatgpt.org.uk/api';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/150.0.0.0 Safari/537.36 Edg/150.0.0.0',
        'Accept' => '*/*',
        'Referer' => 'https://mail.chatgpt.org.uk/zh/',
        'Origin' => 'https://mail.chatgpt.org.uk',
        'DNT' => '1',
    ];

    private static function randomUsername(int $length = 10): string
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
    private static function fetchDomains(): array
    {
        $resp = HttpClient::get(self::BASE_URL . '/domains/public', self::HEADERS);
        $data = HttpClient::json($resp);
        if (empty($data['success'])) {
            throw new \RuntimeException('chatgpt-org-uk: 获取域名失败');
        }
        $domains = [];
        foreach (($data['data']['domains'] ?? []) as $d) {
            if (is_array($d) && ($d['is_active'] ?? null) == 1 && !empty($d['domain_name'])) {
                $domains[] = (string) $d['domain_name'];
            }
        }
        return $domains;
    }

    /**
     * 创建收件箱，返回 [inbox_token, gm_sid]。
     *
     * @return array{0:string,1:string}
     */
    private static function createInbox(string $email): array
    {
        $resp = HttpClient::post(
            self::BASE_URL . '/inbox-token',
            self::HEADERS + ['Content-Type' => 'application/json'],
            json: ['email' => $email],
        );
        $gmSid = '';
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            if (preg_match('/gm_sid=([^;]+)/', $sc, $m)) {
                $gmSid = $m[1];
                break;
            }
        }
        $data = HttpClient::json($resp);
        $token = (string) ($data['auth']['token'] ?? '');
        if ($token === '') {
            throw new \RuntimeException('chatgpt-org-uk: inbox-token 响应缺少 token');
        }
        return [$token, $gmSid];
    }

    public static function generate(): EmailInfo
    {
        $domains = self::fetchDomains();
        if (empty($domains)) {
            throw new \RuntimeException('chatgpt-org-uk: 无可用域名');
        }
        $domain = $domains[array_rand($domains)];
        $email = self::randomUsername(10) . '@' . $domain;
        [$inboxToken, $gmSid] = self::createInbox($email);
        $packed = json_encode(['gmSid' => $gmSid, 'inbox' => $inboxToken]);
        return new EmailInfo('chatgpt-org-uk', $email, $packed);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('chatgpt-org-uk: token 不能为空');
        }
        $gmSid = '';
        $inbox = $token;
        $decoded = json_decode($token, true);
        if (is_array($decoded)) {
            $gmSid = (string) ($decoded['gmSid'] ?? '');
            $inbox = (string) ($decoded['inbox'] ?? '');
        }
        if ($gmSid === '') {
            [$inbox, $gmSid] = self::createInbox($email);
        }

        try {
            return self::fetchEmails($inbox, $gmSid, $email);
        } catch (\Throwable $e) {
            if (str_contains($e->getMessage(), '401') || str_contains($e->getMessage(), '403')) {
                [$newInbox, $newSid] = self::createInbox($email);
                return self::fetchEmails($newInbox, $newSid, $email);
            }
            throw $e;
        }
    }

    /**
     * @return Email[]
     */
    private static function fetchEmails(string $inboxToken, string $gmSid, string $email): array
    {
        $headers = self::HEADERS + ['x-inbox-token' => $inboxToken];
        if ($gmSid !== '') {
            $headers['Cookie'] = 'gm_sid=' . $gmSid;
        }
        $resp = HttpClient::get(self::BASE_URL . '/emails?email=' . rawurlencode($email), $headers);
        $data = HttpClient::json($resp);
        if (empty($data['success'])) {
            throw new \RuntimeException('chatgpt-org-uk: 获取邮件失败');
        }
        $out = [];
        foreach (($data['data']['emails'] ?? []) as $raw) {
            if (is_array($raw)) {
                $out[] = Normalize::email($raw, $email);
            }
        }
        return $out;
    }
}
