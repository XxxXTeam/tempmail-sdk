<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * xkx.me 渠道实现（CSRF + session cookie）— https://xkx.me
 *
 * 创建: GET / 取 CSRF token（meta csrf-token）+ session cookie →
 *       POST /mailbox/create/random（form: _token={csrf}，不跟随重定向）→
 *       从 302 Location 头正则提取邮箱地址（xxx@xkx.me）；
 * 读信: GET /mailbox/{email}/messages（Accept: application/json，携带 Cookie）。
 * token 存储 session cookie 头字符串。
 */
final class XkxMe
{
    private const CHANNEL = 'xkx-me';
    private const BASE_URL = 'https://xkx.me';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36';

    public static function generate(): EmailInfo
    {
        // 获取 CSRF token 和 session cookie
        $resp = HttpClient::get(self::BASE_URL, ['User-Agent' => self::UA], timeout: 15);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('xkx-me: 首页请求失败 ' . $resp->getStatusCode());
        }
        if (preg_match('/csrf-token"\s+content="([^"]+)"/', (string) $resp->getBody(), $m) !== 1) {
            throw new \RuntimeException('xkx-me: 无法获取 CSRF token');
        }
        $csrf = $m[1];
        $cookieHdr = self::cookieHeader($resp);

        // POST 创建邮箱，禁用重定向以读取 Location 头
        $headers = [
            'User-Agent' => self::UA,
            'Content-Type' => 'application/x-www-form-urlencoded',
        ];
        if ($cookieHdr !== '') {
            $headers['Cookie'] = $cookieHdr;
        }
        $resp2 = HttpClient::post(
            self::BASE_URL . '/mailbox/create/random',
            $headers,
            form: ['_token' => $csrf],
            timeout: 15,
            allowRedirects: false,
        );

        $location = $resp2->getHeaderLine('Location');
        if (preg_match('#mailbox/([^/\s"\'<>]+@xkx\.me)#', $location, $m2) !== 1) {
            throw new \RuntimeException('xkx-me: 无法从响应中提取邮箱地址');
        }
        $email = $m2[1];

        return new EmailInfo(self::CHANNEL, $email, $cookieHdr);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('xkx-me: 缺少邮箱地址');
        }

        $headers = [
            'User-Agent' => self::UA,
            'Accept' => 'application/json',
            'X-Requested-With' => 'XMLHttpRequest',
        ];
        $cookieHdr = trim((string) $token);
        if ($cookieHdr !== '') {
            $headers['Cookie'] = $cookieHdr;
        }

        $resp = HttpClient::get(
            self::BASE_URL . '/mailbox/' . rawurlencode($address) . '/messages',
            $headers,
            timeout: 15,
        );
        if ($resp->getStatusCode() === 404) {
            return [];
        }
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('xkx-me: 邮件列表请求失败 ' . $resp->getStatusCode());
        }

        $data = HttpClient::json($resp);

        // 响应可能是 list 或 dict
        if (array_is_list($data)) {
            $messageList = $data;
        } elseif (isset($data['messages']) && is_array($data['messages'])) {
            $messageList = $data['messages'];
        } elseif (isset($data['message']) && is_array($data['message'])) {
            $messageList = [$data['message']];
        } else {
            return [];
        }

        $out = [];
        foreach ($messageList as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $out[] = Normalize::email([
                'id' => $msg['id'] ?? '',
                'from' => $msg['from'] ?? '',
                'to' => $address,
                'subject' => $msg['subject'] ?? '',
                'date' => $msg['date'] ?? '',
                'html' => $msg['html'] ?? ($msg['body'] ?? ''),
                'text' => $msg['text'] ?? '',
                'is_read' => false,
                'attachments' => [],
            ], $address);
        }
        return $out;
    }

    /**
     * 从 Set-Cookie 构建 Cookie 头字符串。
     */
    private static function cookieHeader(ResponseInterface $resp): string
    {
        $d = [];
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first !== '' && str_contains($first, '=')) {
                [$k, $v] = explode('=', $first, 2);
                $k = trim($k);
                if ($k !== '') {
                    $d[$k] = trim($v);
                }
            }
        }
        $parts = [];
        foreach ($d as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }
}
