<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use GuzzleHttp\Psr7\Response;

/**
 * Haribu 渠道实现（https://haribu.net）
 *
 * GET 首页从 <input id="eposta_adres"> 提取邮箱地址并收集 session cookie，
 * token = "haribu1:" + base64(JSON{c:cookieHeader})；
 * getEmails 携带 cookie 请求首页，解析 <li class="mail"> 列表条目。
 */
final class Haribu
{
    private const BASE = 'https://haribu.net';
    private const TOK_PREFIX = 'haribu1:';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
        'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
        'Cache-Control' => 'no-cache',
        'DNT' => '1',
        'Pragma' => 'no-cache',
        'Upgrade-Insecure-Requests' => '1',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    private static function stripTags(string $html): string
    {
        return trim(preg_replace('/<[^>]+>/', ' ', $html) ?? '');
    }

    /**
     * 从响应 Set-Cookie 合并已有 cookie，返回 "k=v; k=v" 串（键排序）。
     */
    private static function mergeCookies(string $prev, Response $resp): string
    {
        $existing = [];
        if ($prev !== '') {
            foreach (explode(';', $prev) as $part) {
                $part = trim($part);
                if (str_contains($part, '=')) {
                    [$k, $v] = explode('=', $part, 2);
                    $k = trim($k);
                    if ($k !== '') {
                        $existing[$k] = trim($v);
                    }
                }
            }
        }
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $nv = trim(explode(';', $sc)[0] ?? '');
            if (str_contains($nv, '=')) {
                [$k, $v] = explode('=', $nv, 2);
                $k = trim($k);
                if ($k !== '') {
                    $existing[$k] = trim($v);
                }
            }
        }
        ksort($existing);
        $parts = [];
        foreach ($existing as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }

    private static function encodeSess(string $cookieHdr): string
    {
        return self::TOK_PREFIX . base64_encode(json_encode(['c' => $cookieHdr]));
    }

    private static function decodeSess(string $token): string
    {
        if (!str_starts_with($token, self::TOK_PREFIX)) {
            throw new \InvalidArgumentException('haribu: 无效的会话令牌');
        }
        $decoded = base64_decode(substr($token, strlen(self::TOK_PREFIX)), true);
        $data = $decoded !== false ? json_decode($decoded, true) : null;
        if (!is_array($data) || empty($data['c'])) {
            throw new \InvalidArgumentException('haribu: 无效的会话令牌');
        }
        return (string) $data['c'];
    }

    private static function extractEmail(string $html): string
    {
        if (
            preg_match(
                '/<input[^>]+id\s*=\s*["\']eposta_adres["\'][^>]+value\s*=\s*["\']([^"\']+)["\']/i',
                $html,
                $m,
            ) === 1
        ) {
            $addr = trim(html_entity_decode($m[1], ENT_QUOTES | ENT_HTML5));
            if ($addr !== '' && str_contains($addr, '@')) {
                return $addr;
            }
        }
        if (
            preg_match(
                '/<input[^>]+value\s*=\s*["\']([^"\']+@[^"\']+)["\'][^>]+id\s*=\s*["\']eposta_adres["\']/i',
                $html,
                $m,
            ) === 1
        ) {
            $addr = trim(html_entity_decode($m[1], ENT_QUOTES | ENT_HTML5));
            if ($addr !== '' && str_contains($addr, '@')) {
                return $addr;
            }
        }
        throw new \RuntimeException('haribu: 未找到邮箱地址（eposta_adres）');
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE, self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('haribu: 首页请求失败 ' . $resp->getStatusCode());
        }
        $html = (string) $resp->getBody();
        if ($html === '') {
            throw new \RuntimeException('haribu: 首页响应为空');
        }
        $email = self::extractEmail($html);
        $cookieHdr = self::mergeCookies('', $resp);
        return new EmailInfo('haribu', $email, self::encodeSess($cookieHdr));
    }

    private static function fetchDetail(string $detailUrl, string $cookieHdr): string
    {
        try {
            $headers = self::HEADERS;
            $headers['Cookie'] = $cookieHdr;
            $headers['Referer'] = self::BASE;
            $resp = HttpClient::get($detailUrl, $headers);
            if ($resp->getStatusCode() < 200 || $resp->getStatusCode() >= 300) {
                return '';
            }
            $html = (string) $resp->getBody();
            $body = self::extractBodyHtml($html);
            if ($body !== '') {
                return $body;
            }
        } catch (\Throwable) {
        }
        return '';
    }

    /**
     * 使用栈式深度匹配提取正文 div 的完整内部 HTML，
     * 避免非贪婪正则在嵌套 div 时截断正文。
     */
    private static function extractBodyHtml(string $page): string
    {
        if (preg_match('/<div\s+(?:id|class)\s*=\s*["\'](?:mail_icerik|icerik|mail-content|message-body)["\'][^>]*>/i', $page, $m, PREG_OFFSET_CAPTURE) !== 1) {
            return '';
        }
        $start = $m[0][1] + strlen($m[0][0]);
        $pos = $start;
        $depth = 1;
        $len = strlen($page);
        while ($pos < $len && $depth > 0) {
            $nextOpen = strpos($page, '<div', $pos);
            $nextClose = strpos($page, '</div>', $pos);
            if ($nextClose === false) break;
            if ($nextOpen !== false && $nextOpen < $nextClose) {
                $depth++;
                $pos = $nextOpen + 4;
            } else {
                $depth--;
                if ($depth === 0) {
                    return trim(substr($page, $start, $nextClose - $start));
                }
                $pos = $nextClose + 6;
            }
        }
        return '';
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $e = trim($email);
        if ($e === '') {
            throw new \InvalidArgumentException('haribu: 邮箱地址为空');
        }
        $cookieHdr = self::decodeSess((string) $token);

        try {
            $kontrol = self::HEADERS;
            $kontrol['Cookie'] = $cookieHdr;
            $kontrol['Referer'] = self::BASE;
            $kontrol['X-Requested-With'] = 'XMLHttpRequest';
            HttpClient::get(self::BASE . '/en/api-kontrol/', $kontrol);
        } catch (\Throwable) {
        }

        $inboxHeaders = self::HEADERS;
        $inboxHeaders['Cookie'] = $cookieHdr;
        $inboxHeaders['Referer'] = self::BASE;
        $resp = HttpClient::get(self::BASE, $inboxHeaders);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('haribu: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $html = (string) $resp->getBody();

        if (
            preg_match_all(
                '/<li\s+class\s*=\s*["\']mail["\'][^>]*>([\s\S]*?)<\/li>/i',
                $html,
                $items,
            ) === false || empty($items[1])
        ) {
            return [];
        }

        $out = [];
        foreach ($items[1] as $idx => $content) {
            $raw = ['id' => 'haribu-' . $idx, 'to' => $e];
            if (preg_match('/<span\s+class\s*=\s*["\']mail_gonderen["\'][^>]*>([\s\S]*?)<\/span>/i', $content, $fm) === 1) {
                $raw['from'] = trim(html_entity_decode(self::stripTags($fm[1]), ENT_QUOTES | ENT_HTML5));
            }
            if (preg_match('/<span\s+class\s*=\s*["\']mail_konu["\'][^>]*>([\s\S]*?)<\/span>/i', $content, $sm) === 1) {
                $raw['subject'] = trim(html_entity_decode(self::stripTags($sm[1]), ENT_QUOTES | ENT_HTML5));
            }
            if (preg_match('/<span\s+class\s*=\s*["\']mail_zaman["\'][^>]*>([\s\S]*?)<\/span>/i', $content, $dm) === 1) {
                $raw['date'] = trim(html_entity_decode(self::stripTags($dm[1]), ENT_QUOTES | ENT_HTML5));
            }
            if (preg_match('/href\s*=\s*["\']([^"\']*(?:mail|read|view)[^"\']*)["\']/i', $content, $lm) === 1) {
                $detailUrl = $lm[1];
                if (!str_starts_with($detailUrl, 'http')) {
                    $detailUrl = self::BASE . '/' . ltrim($detailUrl, '/');
                }
                $body = self::fetchDetail($detailUrl, $cookieHdr);
                if ($body !== '') {
                    $raw['html'] = $body;
                }
            }
            $out[] = Normalize::email($raw, $e);
        }
        return $out;
    }
}
