<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Emailnator 渠道实现 — https://www.emailnator.com
 *
 * XSRF-TOKEN Session 认证：GET / 从 Set-Cookie 提取 XSRF-TOKEN 与 session cookie；
 *   POST /generate-email（JSON {email:[options]}）返回 {email:[addr]}；
 *   POST /message-list（JSON {email}）拉列表，带 messageID 再 POST 拉正文；
 *   token 存 JSON{xsrfToken, cookies}。过滤 messageID 以 "ADS" 开头的广告。
 */
final class Emailnator
{
    private const BASE_URL = 'https://www.emailnator.com';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/145.0.0.0 Safari/537.36';

    /** @var string[] */
    private const EMAIL_OPTIONS = ['plusGmail', 'dotGmail', 'googleMail'];

    /**
     * @return array<string,string>
     */
    private static function defaultHeaders(): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => 'application/json',
            'Content-Type' => 'application/json',
            'X-Requested-With' => 'XMLHttpRequest',
        ];
    }

    /**
     * 初始化 Session，返回 [xsrfToken, cookieStr]。
     *
     * @return array{0:string,1:string}
     */
    private static function initSession(): array
    {
        $resp = HttpClient::get(self::BASE_URL, ['User-Agent' => self::UA]);

        $cookies = [];
        $xsrf = '';
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first === '' || !str_contains($first, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $first, 2);
            $k = trim($k);
            $v = trim($v);
            if ($k === '') {
                continue;
            }
            $decoded = rawurldecode($v);
            $cookies[$k] = $decoded;
            if ($k === 'XSRF-TOKEN') {
                $xsrf = $decoded;
            }
        }
        if ($xsrf === '') {
            throw new \RuntimeException('emailnator: 无法提取 XSRF-TOKEN');
        }
        $parts = [];
        foreach ($cookies as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return [$xsrf, implode('; ', $parts)];
    }

    public static function generate(): EmailInfo
    {
        [$xsrf, $cookies] = self::initSession();

        $resp = HttpClient::post(
            self::BASE_URL . '/generate-email',
            self::defaultHeaders() + ['X-XSRF-TOKEN' => $xsrf, 'Cookie' => $cookies],
            json: ['email' => self::EMAIL_OPTIONS],
        );
        $data = HttpClient::json($resp);
        $emailList = $data['email'] ?? null;
        if (!is_array($emailList) || empty($emailList)) {
            throw new \RuntimeException('emailnator: 生成邮箱失败，响应为空');
        }

        $tokenPayload = (string) json_encode(['xsrfToken' => $xsrf, 'cookies' => $cookies], JSON_UNESCAPED_SLASHES);
        return new EmailInfo('emailnator', (string) $emailList[0], $tokenPayload);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $session = json_decode((string) $token, true);
        if (!is_array($session)) {
            throw new \InvalidArgumentException('emailnator: token 格式无效');
        }
        $xsrf = (string) ($session['xsrfToken'] ?? '');
        $cookies = (string) ($session['cookies'] ?? '');

        $resp = HttpClient::post(
            self::BASE_URL . '/message-list',
            self::defaultHeaders() + ['X-XSRF-TOKEN' => $xsrf, 'Cookie' => $cookies],
            json: ['email' => $email],
        );
        $data = HttpClient::json($resp);
        $messageData = $data['messageData'] ?? null;
        if (!is_array($messageData)) {
            return [];
        }

        $out = [];
        foreach ($messageData as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $msgId = (string) ($msg['messageID'] ?? '');
            if (str_starts_with($msgId, 'ADS')) {
                continue;
            }
            $html = $msgId !== '' ? self::fetchDetail($xsrf, $cookies, $email, $msgId) : '';
            $raw = [
                'id' => $msgId,
                'from' => $msg['from'] ?? '',
                'to' => $email,
                'subject' => $msg['subject'] ?? '',
                'text' => '',
                'html' => $html,
                'date' => $msg['time'] ?? '',
                'isRead' => false,
                'attachments' => [],
            ];
            $out[] = Normalize::email($raw, $email);
        }
        return $out;
    }

    /**
     * POST /message-list 带 messageID 拉取单封 HTML 正文，失败返回空串。
     */
    private static function fetchDetail(string $xsrf, string $cookies, string $email, string $msgId): string
    {
        try {
            $resp = HttpClient::post(
                self::BASE_URL . '/message-list',
                self::defaultHeaders() + ['X-XSRF-TOKEN' => $xsrf, 'Cookie' => $cookies],
                json: ['email' => $email, 'messageID' => $msgId],
            );
        } catch (\Throwable) {
            return '';
        }
        $text = (string) $resp->getBody();
        $decoded = json_decode($text, true);
        if (is_string($decoded)) {
            return $decoded;
        }
        if (is_array($decoded)) {
            return (string) json_encode($decoded, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
        }
        return $text;
    }
}
