<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * tempmailten.com 渠道实现（Laravel + CSRF）— https://tempmailten.com
 *
 * 创建: GET /en 获取 session cookie 与 CSRF token（meta csrf-token）→
 *       POST /messages（body: _token={csrf}&captcha=）取邮箱与邮件列表；
 * 读信: POST /messages 取 messages → GET /view/{id} 取正文。
 * token 序列化保存 CSRF 与 cookie 头（base64）。
 *
 * 与 tempp-mails.com 共享同一模板，核心逻辑参数化后由两渠道复用。
 */
final class TempMailTen
{
    private const CHANNEL = 'tempmailten';
    private const BASE_URL = 'https://tempmailten.com';
    private const TOK_PREFIX = 'tmt1:';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    public static function generate(): EmailInfo
    {
        return self::doGenerate(self::CHANNEL, self::BASE_URL, self::TOK_PREFIX);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        return self::doGetEmails($email, $token, self::CHANNEL, self::BASE_URL, self::TOK_PREFIX);
    }

    /**
     * 参数化的创建实现（供 tempmailten / tempp-mails 共用）。
     */
    public static function doGenerate(string $channel, string $baseUrl, string $tokPrefix): EmailInfo
    {
        $resp = HttpClient::get($baseUrl . '/en', self::browserHeaders());
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException($channel . ': 首页请求失败 ' . $resp->getStatusCode());
        }
        $cookieHdr = self::cookieHeader($resp);
        if (preg_match('/<meta\s+name="csrf-token"\s+content="([^"]+)"/', (string) $resp->getBody(), $m) !== 1) {
            throw new \RuntimeException($channel . ': 未能从首页提取 CSRF token');
        }
        $csrf = $m[1];

        $data = self::postMessages($channel, $baseUrl, $csrf, $cookieHdr);
        $mailbox = trim((string) ($data['mailbox'] ?? ''));
        if ($mailbox === '' || !str_contains($mailbox, '@')) {
            throw new \RuntimeException($channel . ': 邮箱地址无效');
        }
        return new EmailInfo($channel, $mailbox, self::encodeToken($tokPrefix, $csrf, $cookieHdr));
    }

    /**
     * 参数化的读信实现（供 tempmailten / tempp-mails 共用）。
     *
     * @return Email[]
     */
    public static function doGetEmails(string $email, ?string $token, string $channel, string $baseUrl, string $tokPrefix): array
    {
        $email = trim($email);
        if ($email === '') {
            throw new \InvalidArgumentException($channel . ': 邮箱地址为空');
        }
        [$csrf, $cookieHdr] = self::decodeToken($tokPrefix, $channel, trim((string) $token));

        $data = self::postMessages($channel, $baseUrl, $csrf, $cookieHdr);
        $msgs = $data['messages'] ?? null;
        if (!is_array($msgs) || empty($msgs)) {
            return [];
        }

        $out = [];
        foreach ($msgs as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $mid = trim((string) ($msg['id'] ?? ''));
            if ($mid === '') {
                continue;
            }
            $fromAddr = (string) ($msg['from_email'] ?? '');
            $fromName = (string) ($msg['from'] ?? '');
            if ($fromName !== '' && $fromName !== $fromAddr) {
                $fromAddr = $fromName . ' <' . $fromAddr . '>';
            }
            $out[] = Normalize::email([
                'id' => $mid,
                'from' => $fromAddr,
                'to' => $email,
                'subject' => $msg['subject'] ?? '',
                'html' => self::fetchView($baseUrl, $cookieHdr, $mid),
                'isRead' => (bool) ($msg['is_seen'] ?? false),
            ], $email);
        }
        return $out;
    }

    /**
     * POST /messages 获取 {mailbox, messages} JSON。
     *
     * @return array<string,mixed>
     */
    private static function postMessages(string $channel, string $baseUrl, string $csrf, string $cookieHdr): array
    {
        $headers = self::ajaxHeaders($baseUrl);
        $headers['Content-Type'] = 'application/x-www-form-urlencoded';
        $headers['X-CSRF-TOKEN'] = $csrf;
        if ($cookieHdr !== '') {
            $headers['Cookie'] = $cookieHdr;
        }
        $resp = HttpClient::post($baseUrl . '/messages', $headers, form: ['_token' => $csrf, 'captcha' => '']);
        $data = HttpClient::json($resp);
        return $data;
    }

    private static function fetchView(string $baseUrl, string $cookieHdr, string $mid): string
    {
        if ($mid === '') {
            return '';
        }
        $headers = self::browserHeaders();
        $headers['Referer'] = $baseUrl . '/en';
        if ($cookieHdr !== '') {
            $headers['Cookie'] = $cookieHdr;
        }
        try {
            $resp = HttpClient::get($baseUrl . '/view/' . rawurlencode($mid), $headers);
        } catch (\Throwable) {
            return '';
        }
        $status = $resp->getStatusCode();
        if ($status < 200 || $status >= 300) {
            return '';
        }
        return (string) $resp->getBody();
    }

    /**
     * @return array<string,string>
     */
    private static function browserHeaders(): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
        ];
    }

    /**
     * @return array<string,string>
     */
    private static function ajaxHeaders(string $baseUrl): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => 'application/json, text/javascript, */*; q=0.01',
            'Accept-Language' => 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
            'X-Requested-With' => 'XMLHttpRequest',
            'Referer' => $baseUrl . '/en',
        ];
    }

    private static function encodeToken(string $prefix, string $csrf, string $cookieHdr): string
    {
        $raw = (string) json_encode(['t' => $csrf, 'c' => $cookieHdr], JSON_UNESCAPED_SLASHES);
        return $prefix . base64_encode($raw);
    }

    /**
     * @return array{0:string,1:string}
     */
    private static function decodeToken(string $prefix, string $channel, string $token): array
    {
        if (!str_starts_with($token, $prefix)) {
            throw new \InvalidArgumentException($channel . ': 无效的 token');
        }
        $decoded = base64_decode(substr($token, strlen($prefix)), true);
        $o = $decoded !== false ? json_decode($decoded, true) : null;
        if (!is_array($o)) {
            throw new \InvalidArgumentException($channel . ': 无效的 token');
        }
        $csrf = trim((string) ($o['t'] ?? ''));
        $cookieHdr = trim((string) ($o['c'] ?? ''));
        if ($csrf === '') {
            throw new \InvalidArgumentException($channel . ': token 中缺少 CSRF');
        }
        return [$csrf, $cookieHdr];
    }

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
        ksort($d);
        $parts = [];
        foreach ($d as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }
}
