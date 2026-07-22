<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use GuzzleHttp\Psr7\Response;

/**
 * mohmal.com 渠道实现（https://www.mohmal.com）
 *
 * 基于 HTML 解析，使用 connect.sid session cookie 维持会话。
 * generate GET /en/create/random 建立会话并重定向到 /en/inbox，从页面提取 data-email；
 * getEmails GET /en/inbox 解析表格行，逐封 GET /en/message/{id} 取详情。
 * token = "moh1:" + base64(JSON{c:cookieHeader})。
 */
final class Mohmal
{
    private const CHANNEL = 'mohmal';
    private const ORIGIN = 'https://www.mohmal.com';
    private const TOK_PREFIX = 'moh1:';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /** @var array<string,string> */
    private const DEFAULT_HEADERS = [
        'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
        'Accept-Language' => 'en-US,en;q=0.9',
        'Cache-Control' => 'no-cache',
        'DNT' => '1',
        'Pragma' => 'no-cache',
        'Upgrade-Insecure-Requests' => '1',
    ];

    /**
     * @return array<string,string>
     */
    private static function pageHeaders(string $referer, string $cookie = ''): array
    {
        $h = self::DEFAULT_HEADERS;
        $h['User-Agent'] = self::UA;
        $h['Referer'] = $referer;
        if ($cookie !== '') {
            $h['Cookie'] = $cookie;
        }
        return $h;
    }

    private static function stripTags(string $s): string
    {
        return trim(html_entity_decode(preg_replace('/<[^>]+>/', '', $s) ?? '', ENT_QUOTES | ENT_HTML5));
    }

    private static function encodeToken(string $cookieHdr): string
    {
        return self::TOK_PREFIX . base64_encode(json_encode(['c' => $cookieHdr]));
    }

    private static function decodeToken(string $tok): string
    {
        if (!str_starts_with($tok, self::TOK_PREFIX)) {
            throw new \InvalidArgumentException('mohmal: invalid session token');
        }
        $decoded = base64_decode(substr($tok, strlen(self::TOK_PREFIX)), true);
        $o = $decoded !== false ? json_decode($decoded, true) : null;
        if (!is_array($o)) {
            throw new \InvalidArgumentException('mohmal: invalid session token');
        }
        $c = trim((string) ($o['c'] ?? ''));
        if ($c === '') {
            throw new \InvalidArgumentException('mohmal: invalid session token (empty cookie)');
        }
        return $c;
    }

    /**
     * @return array<string,string>
     */
    private static function parseCookieMap(string $hdr): array
    {
        $m = [];
        foreach (explode(';', $hdr) as $part) {
            $part = trim($part);
            if ($part === '' || !str_contains($part, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $part, 2);
            $k = trim($k);
            if ($k !== '') {
                $m[$k] = trim($v);
            }
        }
        return $m;
    }

    private static function mergeCookieHdr(string $prev, Response $resp): string
    {
        $d = self::parseCookieMap($prev);
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $nv = trim(explode(';', $sc)[0] ?? '');
            if (str_contains($nv, '=')) {
                [$k, $v] = explode('=', $nv, 2);
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

    public static function generate(): EmailInfo
    {
        $createUrl = self::ORIGIN . '/en/create/random';
        $inboxUrl = self::ORIGIN . '/en/inbox';

        // 第一步：不跟随重定向以捕获 session cookie
        $r1 = HttpClient::get($createUrl, self::pageHeaders(self::ORIGIN), null, null, false);
        $cookieHdr = self::mergeCookieHdr('', $r1);

        $status = $r1->getStatusCode();
        if (in_array($status, [301, 302, 303, 307, 308], true)) {
            $r2 = HttpClient::get($inboxUrl, self::pageHeaders($createUrl, $cookieHdr));
        } else {
            if ($status >= 400) {
                throw new \RuntimeException('mohmal: create 请求失败 ' . $status);
            }
            $r2 = HttpClient::get($inboxUrl, self::pageHeaders($createUrl, $cookieHdr));
        }
        if ($r2->getStatusCode() >= 400) {
            throw new \RuntimeException('mohmal: inbox 请求失败 ' . $r2->getStatusCode());
        }
        $cookieHdr = self::mergeCookieHdr($cookieHdr, $r2);
        $pageHtml = (string) $r2->getBody();

        $cookies = self::parseCookieMap($cookieHdr);
        if (!isset($cookies['connect.sid'])) {
            throw new \RuntimeException('mohmal: missing connect.sid session cookie');
        }

        if (preg_match('/data-email="([^"]+)"/', $pageHtml, $m) !== 1) {
            throw new \RuntimeException('mohmal: unable to extract email address from inbox page');
        }
        $email = trim(html_entity_decode($m[1], ENT_QUOTES | ENT_HTML5));
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('mohmal: invalid email address: ' . $email);
        }

        return new EmailInfo(self::CHANNEL, $email, self::encodeToken($cookieHdr));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $cookieHdr = self::decodeToken((string) $token);
        $inboxUrl = self::ORIGIN . '/en/inbox';

        $r = HttpClient::get($inboxUrl, self::pageHeaders(self::ORIGIN, $cookieHdr));
        if ($r->getStatusCode() >= 400) {
            throw new \RuntimeException('mohmal: inbox 请求失败 ' . $r->getStatusCode());
        }
        $inboxHtml = (string) $r->getBody();

        if (preg_match_all('#/en/message/(\d+)#', $inboxHtml, $mm) === false || empty($mm[1])) {
            return [];
        }

        $seen = [];
        $uniqueIds = [];
        foreach ($mm[1] as $mid) {
            if (!isset($seen[$mid])) {
                $seen[$mid] = true;
                $uniqueIds[] = $mid;
            }
        }

        $out = [];
        foreach ($uniqueIds as $mid) {
            $detailUrl = self::ORIGIN . '/en/message/' . $mid;
            $rd = HttpClient::get($detailUrl, self::pageHeaders($inboxUrl, $cookieHdr));
            if ($rd->getStatusCode() !== 200) {
                continue;
            }
            $raw = self::parseMessageDetail((string) $rd->getBody(), $mid, $email);
            $out[] = Normalize::email($raw, $email);
        }
        return $out;
    }

    /**
     * @return array<string,string>
     */
    private static function parseMessageDetail(string $page, string $mid, string $recipient): array
    {
        $fromAddr = '';
        if (preg_match('/<span[^>]*class="[^"]*from[^"]*"[^>]*>([\s\S]*?)<\/span>/i', $page, $fm) === 1) {
            $fromRaw = html_entity_decode($fm[1], ENT_QUOTES | ENT_HTML5);
            if (preg_match('/[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}/', $fromRaw, $em) === 1) {
                $fromAddr = $em[0];
            } else {
                $fromAddr = self::stripTags($fromRaw);
            }
        }

        $subject = '';
        if (preg_match('/<span[^>]*class="[^"]*subject[^"]*"[^>]*>([\s\S]*?)<\/span>/i', $page, $sm) === 1) {
            $subject = self::stripTags($sm[1]);
        }

        $date = '';
        if (preg_match('/<span[^>]*class="[^"]*date[^"]*"[^>]*>([\s\S]*?)<\/span>/i', $page, $dm) === 1) {
            $date = self::stripTags($dm[1]);
        }

        $bodyHtml = '';
        if (preg_match('/<div[^>]*class="[^"]*mail-content[^"]*"[^>]*>([\s\S]*?)<\/div>/i', $page, $bm) === 1) {
            $bodyHtml = trim($bm[1]);
        } elseif (preg_match('/<div[^>]*class="[^"]*message-body[^"]*"[^>]*>([\s\S]*?)<\/div>/i', $page, $bm2) === 1) {
            $bodyHtml = trim($bm2[1]);
        }

        return [
            'id' => $mid,
            'from' => $fromAddr,
            'to' => $recipient,
            'subject' => $subject,
            'date' => $date,
            'html' => $bodyHtml,
        ];
    }
}
