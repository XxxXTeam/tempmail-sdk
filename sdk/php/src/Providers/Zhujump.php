<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use GuzzleHttp\Cookie\CookieJar;

/**
 * zhujump 系列固定域渠道实现（jqkjqk.xyz / lyhlevi.com）
 *
 * 通过注册账号、登录获取会话 Cookie 后创建固定域邮箱，
 * 再凭会话 Cookie 拉取邮件列表。会话状态编码进 token（zhj1: 前缀 + base64 JSON）。
 */
final class Zhujump
{
    private const BASE_URL = 'https://mail.zhujump.com';
    private const TOKEN_PREFIX = 'zhj1:';
    private const DEFAULT_EXPIRY_TIME = 3600000;

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36',
    ];

    private static function loginReferer(string $baseUrl): string
    {
        return $baseUrl . '/zh-CN/login';
    }

    /**
     * @return array<string,string>
     */
    private static function headers(string $baseUrl): array
    {
        return self::HEADERS + [
            'Origin' => $baseUrl,
            'Referer' => self::loginReferer($baseUrl),
        ];
    }

    private static function randomValue(string $prefix, int $size): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = $prefix;
        for ($i = 0; $i < $size; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    private static function randomPassword(): string
    {
        return 'Sdk!' . self::randomValue('', 12) . 'A1';
    }

    private static function encodeToken(string $cookie, string $emailId, string $baseUrl): string
    {
        $raw = json_encode(['c' => $cookie, 'i' => $emailId, 'b' => $baseUrl], JSON_UNESCAPED_SLASHES);
        return self::TOKEN_PREFIX . base64_encode((string) $raw);
    }

    /**
     * @return array{cookie:string,email_id:string,base_url:string}
     */
    private static function decodeToken(string $token): array
    {
        if (!str_starts_with($token, self::TOKEN_PREFIX)) {
            throw new \InvalidArgumentException('zhujump: 无效的会话 token');
        }
        $decoded = base64_decode(substr($token, strlen(self::TOKEN_PREFIX)), true);
        if ($decoded === false) {
            throw new \InvalidArgumentException('zhujump: 无效的会话 token');
        }
        $data = json_decode($decoded, true);
        if (!is_array($data)) {
            throw new \InvalidArgumentException('zhujump: 无效的会话 token');
        }
        $cookie = trim((string) ($data['c'] ?? ''));
        $emailId = trim((string) ($data['i'] ?? ''));
        if ($cookie === '' || $emailId === '') {
            throw new \InvalidArgumentException('zhujump: 无效的会话 token');
        }
        $baseUrl = rtrim(trim((string) ($data['b'] ?? self::BASE_URL)), '/');
        return ['cookie' => $cookie, 'email_id' => $emailId, 'base_url' => $baseUrl];
    }

    /**
     * 将 CookieJar 序列化为 "k=v; k=v" 字符串。
     */
    private static function cookieString(CookieJar $jar): string
    {
        $parts = [];
        foreach ($jar->toArray() as $c) {
            $name = $c['Name'] ?? '';
            $value = $c['Value'] ?? '';
            if ($name !== '') {
                $parts[] = $name . '=' . $value;
            }
        }
        return implode('; ', $parts);
    }

    public static function generate(string $domain, string $channel): EmailInfo
    {
        return self::generateForInstance(self::BASE_URL, $domain, $channel, self::DEFAULT_EXPIRY_TIME);
    }

    public static function generateForInstance(string $baseUrl, string $domain, string $channel, ?int $expiryTime = self::DEFAULT_EXPIRY_TIME): EmailInfo
    {
        $baseUrl = rtrim($baseUrl, '/');
        $headers = self::headers($baseUrl);
        $jar = new CookieJar();
        $username = self::randomValue('sdk', 10);
        $password = self::randomPassword();

        // 1. 注册账号
        $reg = HttpClient::post(
            $baseUrl . '/api/auth/register',
            $headers,
            json: ['username' => $username, 'password' => $password, 'turnstileToken' => ''],
            cookies: $jar,
        );
        if ($reg->getStatusCode() >= 400) {
            throw new \RuntimeException('zhujump: 注册失败');
        }

        // 2. 获取 CSRF token
        $csrfResp = HttpClient::get($baseUrl . '/api/auth/csrf', $headers, cookies: $jar);
        if ($csrfResp->getStatusCode() >= 400) {
            throw new \RuntimeException('zhujump: 获取 csrf 失败');
        }
        $csrf = trim((string) (HttpClient::json($csrfResp)['csrfToken'] ?? ''));
        if ($csrf === '') {
            throw new \RuntimeException('zhujump: 缺少 csrf token');
        }

        // 3. 登录（表单，禁止跟随重定向）
        $login = HttpClient::post(
            $baseUrl . '/api/auth/callback/credentials?',
            $headers + ['x-auth-return-redirect' => '1'],
            form: [
                'username' => $username,
                'password' => $password,
                'turnstileToken' => '',
                'redirect' => 'false',
                'csrfToken' => $csrf,
                'callbackUrl' => self::loginReferer($baseUrl),
            ],
            cookies: $jar,
            allowRedirects: false,
        );
        if ($login->getStatusCode() >= 400) {
            throw new \RuntimeException('zhujump: 登录失败');
        }

        // 4. 校验会话
        $sessionResp = HttpClient::get($baseUrl . '/api/auth/session', $headers, cookies: $jar);
        if ($sessionResp->getStatusCode() >= 400) {
            throw new \RuntimeException('zhujump: 获取会话失败');
        }
        $sessionJson = HttpClient::json($sessionResp);
        $sessionUser = is_array($sessionJson['user'] ?? null) ? $sessionJson['user'] : [];
        if (trim((string) ($sessionUser['username'] ?? '')) !== $username) {
            throw new \RuntimeException('zhujump: 登录校验失败');
        }

        // 5. 创建固定域邮箱
        $local = self::randomValue('sdk', 10);
        $body = ['name' => $local, 'domain' => $domain];
        if ($expiryTime !== null) {
            $body['expiryTime'] = $expiryTime;
        }
        $created = HttpClient::post($baseUrl . '/api/emails/generate', $headers, json: $body, cookies: $jar);
        if ($created->getStatusCode() >= 400) {
            throw new \RuntimeException('zhujump: 创建邮箱失败');
        }
        $createdJson = HttpClient::json($created);
        $email = trim((string) ($createdJson['email'] ?? ''));
        $emailId = trim((string) ($createdJson['id'] ?? ''));
        if ($email === '' || $emailId === '') {
            throw new \RuntimeException('zhujump: 创建邮箱响应无效');
        }

        $cookie = self::cookieString($jar);
        return new EmailInfo($channel, $email, self::encodeToken($cookie, $emailId, $baseUrl));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('zhujump: token 不能为空');
        }
        $session = self::decodeToken($token);
        $baseUrl = $session['base_url'];
        $headers = self::headers($baseUrl) + ['Cookie' => $session['cookie']];

        $resp = HttpClient::get($baseUrl . '/api/emails/' . rawurlencode($session['email_id']), $headers);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('zhujump: 拉取邮件失败');
        }
        $data = HttpClient::json($resp);
        $rows = $data['messages'] ?? null;
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $source = $item;
            $messageId = trim((string) ($item['id'] ?? ''));
            $hasBody = trim((string) ($item['content'] ?? '')) !== '' || trim((string) ($item['html'] ?? '')) !== '';
            if ($messageId !== '' && !$hasBody) {
                $detail = self::fetchDetail($baseUrl, $session['cookie'], $session['email_id'], $messageId);
                if ($detail !== null) {
                    $source = array_merge($item, $detail);
                }
            }
            $out[] = Normalize::email([
                'id' => $source['id'] ?? '',
                'from_address' => $source['from_address'] ?? '',
                'to_address' => $source['to_address'] ?? $email,
                'subject' => $source['subject'] ?? '',
                'content' => $source['content'] ?? '',
                'html' => $source['html'] ?? '',
                'received_at' => $source['received_at'] ?? ($source['sent_at'] ?? null),
                'isRead' => false,
            ], $email);
        }
        return $out;
    }

    /**
     * @return array<mixed>|null
     */
    private static function fetchDetail(string $baseUrl, string $cookie, string $emailId, string $messageId): ?array
    {
        try {
            $resp = HttpClient::get(
                $baseUrl . '/api/emails/' . rawurlencode($emailId) . '/' . rawurlencode($messageId),
                self::headers($baseUrl) + ['Cookie' => $cookie],
            );
            if ($resp->getStatusCode() >= 400) {
                return null;
            }
            $data = HttpClient::json($resp);
            $message = $data['message'] ?? null;
            return is_array($message) ? $message : null;
        } catch (\Throwable) {
            return null;
        }
    }
}
