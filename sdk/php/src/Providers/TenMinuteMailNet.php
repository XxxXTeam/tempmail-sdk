<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use GuzzleHttp\Psr7\Response;

/**
 * 10minutemail.net 渠道实现（https://10minutemail.net）
 *
 * generate GET / 获取 PHPSESSID 并从输入框正则提取随机分配邮箱地址；
 * getEmails GET /mailbox.ajax.php?_={毫秒时间戳} 解析 HTML 表格，
 * 逐封 GET /readmail.html?mid={id} 提取 mailinhtml 正文片段。
 * token = "tmn1:" + base64(JSON{c:cookieHeader})。
 */
final class TenMinuteMailNet
{
    private const CHANNEL = '10minutemail-net';
    private const BASE_URL = 'https://10minutemail.net';
    private const TOK_PREFIX = 'tmn1:';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * @return array<string,string>
     */
    private static function browserHeaders(): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,'
                . 'image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
        ];
    }

    /**
     * @return array<string,string>
     */
    private static function ajaxHeaders(): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => '*/*',
            'Accept-Language' => 'en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7',
            'X-Requested-With' => 'XMLHttpRequest',
            'Referer' => self::BASE_URL . '/',
        ];
    }

    private static function cookieHeader(Response $resp): string
    {
        $d = [];
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

    private static function encodeToken(string $cookieHdr): string
    {
        return self::TOK_PREFIX . base64_encode(json_encode(['c' => $cookieHdr]));
    }

    private static function decodeToken(string $token): string
    {
        if (!str_starts_with($token, self::TOK_PREFIX)) {
            return '';
        }
        $decoded = base64_decode(substr($token, strlen(self::TOK_PREFIX)), true);
        $o = $decoded !== false ? json_decode($decoded, true) : null;
        if (!is_array($o)) {
            return '';
        }
        return trim((string) ($o['c'] ?? ''));
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL . '/', self::browserHeaders());
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('10minutemail-net: 首页请求失败 ' . $resp->getStatusCode());
        }
        $cookieHdr = self::cookieHeader($resp);

        if (preg_match('/value="([^"]+@[^"]+)"/', (string) $resp->getBody(), $m) !== 1) {
            throw new \RuntimeException('10minutemail-net: 未能从首页提取邮箱地址');
        }
        $email = trim($m[1]);
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('10minutemail-net: 获取到的邮箱地址无效: ' . $email);
        }
        return new EmailInfo(self::CHANNEL, $email, self::encodeToken($cookieHdr));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $email = trim($email);
        if ($email === '') {
            throw new \InvalidArgumentException('10minutemail-net: 邮箱地址为空');
        }
        $cookieHdr = self::decodeToken(trim((string) $token));

        $headers = self::ajaxHeaders();
        if ($cookieHdr !== '') {
            $headers['Cookie'] = $cookieHdr;
        }
        $listUrl = self::BASE_URL . '/mailbox.ajax.php?_=' . (int) round(microtime(true) * 1000);
        $resp = HttpClient::get($listUrl, $headers);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('10minutemail-net: 列表请求失败 ' . $resp->getStatusCode());
        }

        $out = [];
        if (preg_match_all('/<tr[^>]*>(.*?)<\/tr>/is', (string) $resp->getBody(), $rowMatches, PREG_SET_ORDER) === false) {
            return [];
        }
        foreach ($rowMatches as $rm) {
            $rowFull = $rm[0];
            $rowInner = $rm[1];
            if (str_contains(strtolower($rowInner), '<th')) {
                continue;
            }
            if (preg_match('/readmail\.html\?mid=([^\'"&]+)/i', $rowInner, $midMatch) !== 1) {
                continue;
            }
            $mailId = trim($midMatch[1]);
            if ($mailId === '') {
                continue;
            }

            $cells = [];
            if (preg_match_all('/<td[^>]*>(.*?)<\/td>/is', $rowInner, $cm) !== false) {
                $cells = $cm[1];
            }
            $fromCell = $cells[0] ?? '';
            $subjectCell = $cells[1] ?? '';
            $dateCell = $cells[2] ?? '';

            $fromAddr = self::extractText($fromCell);
            $subject = self::extractText($subjectCell);

            if (preg_match('/title="([^"]+)"/i', $dateCell, $tm) === 1) {
                $date = trim($tm[1]);
            } else {
                $date = self::extractText($dateCell);
            }

            $isRead = !str_contains(strtolower($rowFull), 'font-weight: bold');

            $raw = [
                'id' => $mailId,
                'from' => $fromAddr,
                'to' => $email,
                'subject' => $subject,
                'html' => self::fetchBody($cookieHdr, $mailId),
                'date' => $date,
                'isRead' => $isRead,
            ];
            $out[] = Normalize::email($raw, $email);
        }
        return $out;
    }

    private static function fetchBody(string $cookieHdr, string $mailId): string
    {
        if ($mailId === '') {
            return '';
        }
        $headers = self::browserHeaders();
        $headers['Referer'] = self::BASE_URL . '/';
        if ($cookieHdr !== '') {
            $headers['Cookie'] = $cookieHdr;
        }
        try {
            $resp = HttpClient::get(self::BASE_URL . '/readmail.html?mid=' . $mailId, $headers);
        } catch (\Throwable) {
            return '';
        }
        $status = $resp->getStatusCode();
        if ($status < 200 || $status >= 300) {
            return '';
        }
        return self::extractBody((string) $resp->getBody());
    }

    private static function extractBody(string $page): string
    {
        $startMark = 'class="mailinhtml"';
        $si = strpos($page, $startMark);
        if ($si === false) {
            return '';
        }
        $gt = strpos($page, '>', $si);
        if ($gt === false) {
            return '';
        }
        $rest = substr($page, $gt + 1);

        $ei = strpos($rest, 'email-decode.min.js');
        if ($ei === false) {
            $di = strpos($rest, '</div>');
            if ($di === false) {
                return trim($rest);
            }
            return trim(substr($rest, 0, $di));
        }

        $segment = substr($rest, 0, $ei);
        $sIdx = strrpos($segment, '<script');
        if ($sIdx !== false) {
            $segment = substr($segment, 0, $sIdx);
        }
        $segment = trim($segment);
        for ($i = 0; $i < 2; $i++) {
            $segment = trim($segment);
            if (str_ends_with($segment, '</div>')) {
                $segment = substr($segment, 0, -strlen('</div>'));
            }
        }
        return trim($segment);
    }

    private static function extractText(string $cell): string
    {
        if (preg_match('/data-cfemail="([0-9a-fA-F]+)"/i', $cell, $cf) === 1) {
            $decoded = self::cfDecode($cf[1]);
            if ($decoded !== '') {
                return $decoded;
            }
        }
        $text = preg_replace('/<[^>]+>/s', '', $cell) ?? '';
        $replacements = [
            '&nbsp;' => ' ',
            '&#160;' => ' ',
            '&amp;' => '&',
            '&lt;' => '<',
            '&gt;' => '>',
            '&quot;' => '"',
        ];
        $text = strtr($text, $replacements);
        return trim($text);
    }

    private static function cfDecode(string $encoded): string
    {
        $data = @hex2bin($encoded);
        if ($data === false || strlen($data) < 2) {
            return '';
        }
        $key = ord($data[0]);
        $decoded = '';
        for ($i = 1, $len = strlen($data); $i < $len; $i++) {
            $decoded .= chr(ord($data[$i]) ^ $key);
        }
        if (!str_contains($decoded, '@')) {
            return '';
        }
        return $decoded;
    }
}
