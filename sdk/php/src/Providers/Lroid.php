<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Lroid 渠道实现（https://lroid.com）
 *
 * GET 首页由服务端自动分配邮箱地址（<input id="eposta_adres">），
 * session cookie 序列化为 JSON token{cookie}；
 * getEmails 先尝试 /en/api-kontrol/ JSON，失败回退 HTML 页面解析。
 */
final class Lroid
{
    private const BASE_URL = 'https://lroid.com';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
        'Accept-Language' => 'en-US,en;q=0.9',
        'Referer' => self::BASE_URL . '/',
        'User-Agent' => self::UA,
    ];

    /**
     * 从 Set-Cookie 头收集 cookie，序列化为 "k=v; k=v"。
     *
     * @param string[] $setCookies
     */
    private static function cookieHeader(array $setCookies): string
    {
        $map = [];
        foreach ($setCookies as $sc) {
            $nv = trim(explode(';', $sc)[0] ?? '');
            if (str_contains($nv, '=')) {
                [$k, $v] = explode('=', $nv, 2);
                $k = trim($k);
                if ($k !== '') {
                    $map[$k] = trim($v);
                }
            }
        }
        $parts = [];
        foreach ($map as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }

    private static function extractEmail(string $html): string
    {
        if (
            preg_match(
                '/<input[^>]+id=["\']eposta_adres["\'][^>]+value=["\']([^"\']+@[^"\']+)["\']/i',
                $html,
                $m,
            ) !== 1
        ) {
            preg_match(
                '/<input[^>]+value=["\']([^"\']+@[^"\']+)["\'][^>]+id=["\']eposta_adres["\']/i',
                $html,
                $m,
            );
        }
        if (empty($m[1])) {
            throw new \RuntimeException('lroid: 无法从 HTML 响应中解析邮箱地址');
        }
        $addr = trim($m[1]);
        if (!str_contains($addr, '@')) {
            throw new \RuntimeException('lroid: 解析到的邮箱地址无效');
        }
        return $addr;
    }

    private static function decodeToken(string $token): string
    {
        $data = json_decode($token, true);
        if (!is_array($data) || empty($data['cookie'])) {
            throw new \InvalidArgumentException('lroid: 无效的 session token');
        }
        return (string) $data['cookie'];
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL, self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('lroid: 首页请求失败 ' . $resp->getStatusCode());
        }
        $email = self::extractEmail((string) $resp->getBody());
        $cookie = self::cookieHeader($resp->getHeader('Set-Cookie'));
        $token = json_encode(['cookie' => $cookie]);
        return new EmailInfo('lroid', $email, $token);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('lroid: token 不能为空');
        }
        $cookie = self::decodeToken($token);
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('lroid: 邮箱地址不能为空');
        }

        // 尝试 kontrol API（JSON）
        try {
            $resp = HttpClient::get(self::BASE_URL . '/en/api-kontrol/', [
                'Accept' => 'application/json, text/html, */*;q=0.8',
                'Referer' => self::BASE_URL . '/',
                'Cookie' => $cookie,
                'User-Agent' => self::UA,
            ]);
            if ($resp->getStatusCode() < 400) {
                $data = json_decode((string) $resp->getBody(), true);
                if (is_array($data)) {
                    if (array_is_list($data)) {
                        return self::parseJsonEmails($data, $address);
                    }
                    foreach (['mails', 'emails', 'messages', 'data', 'inbox'] as $key) {
                        if (is_array($data[$key] ?? null)) {
                            return self::parseJsonEmails($data[$key], $address);
                        }
                    }
                    if (!empty($data['id']) || !empty($data['subject']) || !empty($data['from'])) {
                        return self::parseJsonEmails([$data], $address);
                    }
                }
            }
        } catch (\Throwable) {
        }

        return self::parseHtmlEmails($cookie, $address);
    }

    /**
     * @param array<mixed> $items
     * @return Email[]
     */
    private static function parseJsonEmails(array $items, string $recipient): array
    {
        $out = [];
        foreach ($items as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $normalized = $raw;
            if (isset($normalized['body']) && !isset($normalized['html'])) {
                $normalized['html'] = $normalized['body'];
            }
            if (isset($normalized['content']) && !isset($normalized['html'])) {
                $normalized['html'] = $normalized['content'];
            }
            $out[] = Normalize::email($normalized, $recipient);
        }
        return $out;
    }

    /**
     * @return Email[]
     */
    private static function parseHtmlEmails(string $cookie, string $recipient): array
    {
        $resp = HttpClient::get(self::BASE_URL, [
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Referer' => self::BASE_URL . '/',
            'Cookie' => $cookie,
            'User-Agent' => self::UA,
        ]);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('lroid: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $html = (string) $resp->getBody();

        if (
            preg_match_all(
                '/<li[^>]*class=["\'][^"\']*\bmail\b[^"\']*["\'][^>]*>(.*?)<\/li>/is',
                $html,
                $items,
            ) === false || empty($items[1])
        ) {
            return [];
        }

        $out = [];
        foreach ($items[1] as $idx => $itemHtml) {
            $mailId = '';
            if (preg_match('/href=["\']\/?([^"\']*?\/?\d+)["\']/i', $itemHtml, $lm) === 1) {
                $seg = rtrim(trim($lm[1]), '/');
                $pos = strrpos($seg, '/');
                $mailId = $pos === false ? $seg : substr($seg, $pos + 1);
            }
            if ($mailId === '' && preg_match('/data-id=["\']([^"\']+)["\']/i', $itemHtml, $dm) === 1) {
                $mailId = trim($dm[1]);
            }
            if ($mailId === '') {
                $mailId = (string) ($idx + 1);
            }

            $subject = '';
            if (preg_match('/class=["\'][^"\']*\bsubject\b[^"\']*["\'][^>]*>(.*?)<\//is', $itemHtml, $sm) === 1) {
                $subject = trim(preg_replace('/<[^>]+>/', '', $sm[1]) ?? '');
            }
            $fromAddr = '';
            if (preg_match('/class=["\'][^"\']*\b(?:from|sender)\b[^"\']*["\'][^>]*>(.*?)<\//is', $itemHtml, $fm) === 1) {
                $fromAddr = trim(preg_replace('/<[^>]+>/', '', $fm[1]) ?? '');
            }
            $date = '';
            if (preg_match('/class=["\'][^"\']*\b(?:date|time)\b[^"\']*["\'][^>]*>(.*?)<\//is', $itemHtml, $dtm) === 1) {
                $date = trim(preg_replace('/<[^>]+>/', '', $dtm[1]) ?? '');
            }

            $bodyHtml = '';
            $bodyText = '';
            if (ctype_digit($mailId)) {
                try {
                    $detail = self::fetchMailDetail($cookie, $mailId);
                    $bodyHtml = $detail['html'];
                    $bodyText = $detail['text'];
                    if ($subject === '' && $detail['subject'] !== '') {
                        $subject = $detail['subject'];
                    }
                    if ($fromAddr === '' && $detail['from'] !== '') {
                        $fromAddr = $detail['from'];
                    }
                    if ($date === '' && $detail['date'] !== '') {
                        $date = $detail['date'];
                    }
                } catch (\Throwable) {
                }
            }

            $out[] = new Email(
                id: $mailId,
                fromAddr: $fromAddr,
                to: $recipient,
                subject: $subject,
                text: $bodyText,
                html: $bodyHtml,
                date: $date,
                isRead: false,
                attachments: [],
            );
        }
        return $out;
    }

    /**
     * @return array{html:string,text:string,subject:string,from:string,date:string}
     */
    private static function fetchMailDetail(string $cookie, string $mailId): array
    {
        $resp = HttpClient::get(self::BASE_URL . '/' . $mailId, [
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Referer' => self::BASE_URL . '/',
            'Cookie' => $cookie,
            'User-Agent' => self::UA,
        ]);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('lroid: 邮件详情请求失败 ' . $resp->getStatusCode());
        }
        $html = (string) $resp->getBody();
        $result = ['html' => '', 'text' => '', 'subject' => '', 'from' => '', 'date' => ''];

        if (preg_match('/<iframe[^>]+srcdoc=["\']([^"\']+)["\']/is', $html, $ifm) === 1) {
            $result['html'] = html_entity_decode($ifm[1], ENT_QUOTES | ENT_HTML5);
        } elseif (preg_match('/<iframe[^>]+src=["\']([^"\']+)["\']/is', $html, $ism) === 1) {
            $src = $ism[1];
            if (!str_starts_with($src, 'http')) {
                $src = self::BASE_URL . '/' . ltrim($src, '/');
            }
            try {
                $ir = HttpClient::get($src, [
                    'Accept' => 'text/html, */*',
                    'Referer' => self::BASE_URL . '/' . $mailId,
                    'Cookie' => $cookie,
                    'User-Agent' => self::UA,
                ]);
                if ($ir->getStatusCode() < 400) {
                    $result['html'] = (string) $ir->getBody();
                }
            } catch (\Throwable) {
            }
        }

        if ($result['html'] === '') {
            if (
                preg_match(
                    '/class=["\'][^"\']*\b(?:mail-body|mail-content|message-body|content)\b[^"\']*["\'][^>]*>(.*?)<\/div>/is',
                    $html,
                    $bm,
                ) === 1
            ) {
                $result['html'] = trim($bm[1]);
            }
        }
        if ($result['html'] !== '') {
            $result['text'] = trim(preg_replace('/<[^>]+>/', '', $result['html']) ?? '');
        }
        if (preg_match('/class=["\'][^"\']*\bsubject\b[^"\']*["\'][^>]*>(.*?)<\//is', $html, $sm) === 1) {
            $result['subject'] = trim(preg_replace('/<[^>]+>/', '', $sm[1]) ?? '');
        }
        if (preg_match('/class=["\'][^"\']*\b(?:from|sender)\b[^"\']*["\'][^>]*>(.*?)<\//is', $html, $fm) === 1) {
            $result['from'] = trim(preg_replace('/<[^>]+>/', '', $fm[1]) ?? '');
        }
        if (preg_match('/class=["\'][^"\']*\b(?:date|time)\b[^"\']*["\'][^>]*>(.*?)<\//is', $html, $dm) === 1) {
            $result['date'] = trim(preg_replace('/<[^>]+>/', '', $dm[1]) ?? '');
        }
        return $result;
    }
}
