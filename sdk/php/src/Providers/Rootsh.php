<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * Rootsh(BccTo) 渠道实现 — https://rootsh.com
 *
 * 需要 cookie session 认证，token 序列化保存 lastCheckTime 与 cookie 头。
 * 创建: GET / 获取 session cookie → POST /applymail 申请邮箱；
 * 读信: POST /getmail 获取列表 → POST /viewmail 获取正文。
 */
final class Rootsh
{
    private const CHANNEL = 'rootsh';
    private const BASE = 'https://rootsh.com';
    private const DEFAULT_DOMAIN = 'bccto.cc';
    private const TOK_PREFIX = 'rootsh1:';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * @return array<string,string>
     */
    private static function apiHeaders(): array
    {
        return [
            'Accept' => 'application/json, text/plain, */*',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
            'Cache-Control' => 'no-cache',
            'DNT' => '1',
            'Pragma' => 'no-cache',
            'Referer' => self::BASE . '/',
            'X-Requested-With' => 'XMLHttpRequest',
            'User-Agent' => self::UA,
        ];
    }

    private static function randomLocal(int $length = 10): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        $dom = trim((string) $domain) !== '' ? trim((string) $domain) : self::DEFAULT_DOMAIN;
        $mailAddr = self::randomLocal() . '@' . $dom;

        // 第一步：GET 首页获取 session cookie
        $r1 = HttpClient::get(self::BASE . '/', [
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
            'User-Agent' => self::UA,
        ]);
        if ($r1->getStatusCode() >= 400) {
            throw new \RuntimeException('rootsh: 首页请求失败 ' . $r1->getStatusCode());
        }
        $cookieHdr = self::mergeCookieHdr('', $r1);

        // 第二步：POST /applymail 申请邮箱
        $r2 = HttpClient::post(
            self::BASE . '/applymail',
            self::apiHeaders() + [
                'Content-Type' => 'application/x-www-form-urlencoded; charset=UTF-8',
                'Cookie' => $cookieHdr,
            ],
            body: 'mail=' . rawurlencode($mailAddr),
        );
        $cookieHdr = self::mergeCookieHdr($cookieHdr, $r2);
        $body = HttpClient::json($r2);

        if ((string) ($body['success'] ?? '') !== 'true') {
            $tips = (string) ($body['tips'] ?? '');
            throw new \RuntimeException('rootsh: 邮箱申请失败: ' . $tips);
        }

        $actualEmail = trim((string) ($body['user'] ?? '')) !== '' ? trim((string) $body['user']) : $mailAddr;
        $lastCheckTime = $body['time'] ?? 0;

        return new EmailInfo(self::CHANNEL, $actualEmail, self::encodeToken($lastCheckTime, $cookieHdr));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('rootsh: token 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('rootsh: 邮箱地址不能为空');
        }
        [$lastCheckTime, $cookieHdr] = self::decodeToken($token);

        $ts = (int) round(microtime(true) * 1000);
        $r = HttpClient::post(
            self::BASE . '/getmail',
            self::apiHeaders() + [
                'Content-Type' => 'application/x-www-form-urlencoded; charset=UTF-8',
                'Cookie' => $cookieHdr,
            ],
            body: 'mail=' . rawurlencode($addr) . '&time=' . rawurlencode((string) $lastCheckTime) . '&_=' . $ts,
        );
        $body = HttpClient::json($r);
        if ((string) ($body['success'] ?? '') !== 'true') {
            return [];
        }
        $mailList = $body['mail'] ?? null;
        if (!is_array($mailList)) {
            return [];
        }

        $out = [];
        foreach ($mailList as $item) {
            // 每个 item 是数组: [display_name, from_email, subject, date_str, mail_fid, ...]
            if (!is_array($item) || count($item) < 5) {
                continue;
            }
            $displayName = (string) ($item[0] ?? '');
            $fromEmail = (string) ($item[1] ?? '');
            $subject = (string) ($item[2] ?? '');
            $dateStr = (string) ($item[3] ?? '');
            $mailFid = (string) ($item[4] ?? '');
            $fromAddr = $fromEmail !== '' ? $fromEmail : $displayName;

            $htmlContent = '';
            if ($mailFid !== '') {
                try {
                    $rv = HttpClient::post(
                        self::BASE . '/viewmail',
                        self::apiHeaders() + [
                            'Content-Type' => 'application/x-www-form-urlencoded; charset=UTF-8',
                            'Cookie' => $cookieHdr,
                        ],
                        body: 'mail=' . rawurlencode($mailFid) . '&to=' . rawurlencode($addr),
                    );
                    if ($rv->getStatusCode() === 200) {
                        $vb = HttpClient::json($rv);
                        if ((string) ($vb['success'] ?? '') === 'true') {
                            $htmlContent = (string) ($vb['mail'] ?? '');
                        }
                    }
                } catch (\Throwable) {
                }
            }

            $out[] = Normalize::email([
                'id' => $mailFid,
                'from' => $fromAddr,
                'to' => $addr,
                'subject' => $subject,
                'date' => $dateStr,
                'html' => $htmlContent,
            ], $addr);
        }
        return $out;
    }

    private static function encodeToken(mixed $lastCheckTime, string $cookieHdr): string
    {
        $payload = (string) json_encode(['t' => $lastCheckTime, 'c' => $cookieHdr], JSON_UNESCAPED_SLASHES);
        return self::TOK_PREFIX . $payload;
    }

    /**
     * @return array{0:mixed,1:string}
     */
    private static function decodeToken(string $token): array
    {
        if (!str_starts_with($token, self::TOK_PREFIX)) {
            throw new \InvalidArgumentException('rootsh: 无效的 session token');
        }
        $data = json_decode(substr($token, strlen(self::TOK_PREFIX)), true);
        if (!is_array($data)) {
            throw new \InvalidArgumentException('rootsh: 无效的 session token');
        }
        return [$data['t'] ?? 0, (string) ($data['c'] ?? '')];
    }

    /**
     * 将响应 Set-Cookie 合并进已有 Cookie 头。
     */
    private static function mergeCookieHdr(string $prev, ResponseInterface $resp): string
    {
        $d = [];
        foreach (explode(';', $prev) as $part) {
            $part = trim($part);
            if ($part !== '' && str_contains($part, '=')) {
                [$k, $v] = explode('=', $part, 2);
                $d[trim($k)] = trim($v);
            }
        }
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
