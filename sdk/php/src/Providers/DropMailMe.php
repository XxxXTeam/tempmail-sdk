<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * dropmail-me 渠道实现（https://dropmail.me）
 *
 * 自行合成认证 token：从页面提取 data-k → 反转 + base64 解码得 secret →
 * FNV-1a 哈希构造 website_ 令牌；GraphQL introduceSession 建会话，
 * 复合 token = JSON{session_id, auth_token}。
 */
final class DropMailMe
{
    private const BASE_URL = 'https://dropmail.me';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => self::UA,
        'Accept' => 'application/json',
        'Content-Type' => 'application/json',
    ];

    /**
     * FNV-1a 变体哈希（与 Python 实现逐位对齐）。
     */
    private static function fnvHash(string $s): string
    {
        $h = 2166136261;
        $len = strlen($s);
        for ($i = 0; $i < $len; $i++) {
            $h ^= ord($s[$i]);
            $h += ($h << 1) + ($h << 4) + ($h << 7) + ($h << 8) + ($h << 24);
            $h &= 0xFFFFFFFF;
        }
        return dechex($h);
    }

    /**
     * 生成 dropmail.me API 认证 token。
     */
    private static function generateAuthToken(): string
    {
        $resp = HttpClient::get(self::BASE_URL . '/en/', [
            'User-Agent' => self::UA,
            'Accept' => 'text/html',
        ]);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('dropmail-me: 首页请求失败 ' . $resp->getStatusCode());
        }
        $html = (string) $resp->getBody();
        if (preg_match('/<meta\s+name="app-config"\s+data-k="([^"]+)"/', $html, $m) !== 1) {
            throw new \RuntimeException('dropmail-me: 无法从页面提取 data-k');
        }
        $secret = base64_decode(strrev($m[1]));
        if ($secret === false) {
            throw new \RuntimeException('dropmail-me: data-k 解码失败');
        }
        $dateStr = gmdate('Ymd');
        $chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
        $randomPart = $dateStr;
        for ($i = 0; $i < 16; $i++) {
            $randomPart .= $chars[random_int(0, strlen($chars) - 1)];
        }
        $h = self::fnvHash($randomPart . $secret);
        return 'website_' . $randomPart . '_' . $h;
    }

    public static function generate(): EmailInfo
    {
        $authToken = self::generateAuthToken();
        $query = 'mutation { introduceSession { id expiresAt addresses { address } } }';
        $resp = HttpClient::post(
            self::BASE_URL . '/api/graphql/' . $authToken,
            self::HEADERS,
            json: ['query' => $query],
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('dropmail-me: 创建 session 失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $session = $data['data']['introduceSession'] ?? null;
        if (!is_array($session)) {
            throw new \RuntimeException('dropmail-me: 创建 session 失败');
        }
        $sessionId = (string) ($session['id'] ?? '');
        $address = (string) ($session['addresses'][0]['address'] ?? '');
        if ($sessionId === '' || $address === '') {
            throw new \RuntimeException('dropmail-me: 响应缺少 session ID 或地址');
        }

        $expiresAt = null;
        $expiresStr = $session['expiresAt'] ?? null;
        if (is_string($expiresStr) && $expiresStr !== '') {
            $ts = strtotime($expiresStr);
            if ($ts !== false) {
                $expiresAt = $ts * 1000;
            }
        }

        $composite = json_encode(['session_id' => $sessionId, 'auth_token' => $authToken]);
        return new EmailInfo('dropmail-me', $address, $composite, expiresAt: $expiresAt);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('dropmail-me: token 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('dropmail-me: 邮箱地址不能为空');
        }
        $tokenData = json_decode($token, true);
        if (!is_array($tokenData)) {
            throw new \InvalidArgumentException('dropmail-me: token 格式无效，应为 JSON 字符串');
        }
        $sessionId = (string) ($tokenData['session_id'] ?? '');
        $authToken = (string) ($tokenData['auth_token'] ?? '');
        if ($sessionId === '' || $authToken === '') {
            throw new \InvalidArgumentException('dropmail-me: token 中缺少 session_id 或 auth_token');
        }

        $query = '{ session(id:"' . $sessionId . '") '
            . '{ mails { id headerFrom headerSubject text html receivedAt } } }';
        $resp = HttpClient::post(
            self::BASE_URL . '/api/graphql/' . $authToken,
            self::HEADERS,
            json: ['query' => $query],
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('dropmail-me: 读信失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $mails = $data['data']['session']['mails'] ?? null;
        if (!is_array($mails)) {
            return [];
        }
        $out = [];
        foreach ($mails as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $out[] = Normalize::email([
                'id' => $msg['id'] ?? '',
                'from' => $msg['headerFrom'] ?? '',
                'to' => $addr,
                'subject' => $msg['headerSubject'] ?? '',
                'text' => $msg['text'] ?? '',
                'html' => $msg['html'] ?? '',
                'date' => $msg['receivedAt'] ?? '',
            ], $addr);
        }
        return $out;
    }
}
