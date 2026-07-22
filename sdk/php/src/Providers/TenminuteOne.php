<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * 10minutemail.one 渠道实现（含 xghff.com 等 6 个固定域别名）
 *
 * 从 SSR 页面 __NUXT_DATA__ 中解析 mailServiceToken（JWT）与 emailDomains，
 * 收信 GET https://web.10minutemail.one/api/v1/mailbox/{email}，需 Bearer 鉴权。
 */
final class TenminuteOne
{
    private const SITE_ORIGIN = 'https://10minutemail.one';
    private const API_BASE = 'https://web.10minutemail.one/api/v1';
    private const DEFAULT_UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36';

    /** @var string[] */
    private const KNOWN_EMAIL_DOMAINS = [
        'xghff.com',
        'oqqaj.com',
        'psovv.com',
        'dbwot.com',
        'ygwpr.com',
        'imxwe.com',
    ];

    /**
     * 对邮箱地址做 mailbox 路径编码：@ 固定编码为 %40，本地部分与域名分别编码。
     */
    private static function encMailboxEmail(string $email): string
    {
        $at = strpos($email, '@');
        if ($at !== false) {
            $local = substr($email, 0, $at);
            $dom = substr($email, $at + 1);
            return rawurlencode($local) . '%40' . rawurlencode($dom);
        }
        return rawurlencode($email);
    }

    /**
     * 解析 __NUXT_DATA__ script 中的 JSON 数组。
     *
     * @return array<mixed>
     */
    private static function parseNuxtArray(string $html): array
    {
        if (!preg_match('/<script[^>]*\bid="__NUXT_DATA__"[^>]*>([\s\S]*?)<\/script>/i', $html, $m)) {
            throw new \RuntimeException('10minute-one: 未找到 __NUXT_DATA__');
        }
        $arr = json_decode(trim($m[1]), true);
        if (!is_array($arr)) {
            throw new \RuntimeException('10minute-one: __NUXT_DATA__ 解析失败');
        }
        return $arr;
    }

    /**
     * NUXT 扁平数组中整型值为对其他槽位的引用，递归解引用。
     *
     * @param array<mixed> $arr
     */
    private static function resolveRef(array $arr, mixed $value, int $depth = 0): mixed
    {
        if ($depth > 64 || is_bool($value)) {
            return $value;
        }
        if (is_int($value) && $value >= 0 && $value < count($arr)) {
            return self::resolveRef($arr, $arr[$value], $depth + 1);
        }
        return $value;
    }

    private static function isJwt(mixed $v): bool
    {
        return is_string($v) && (bool) preg_match('/^[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+$/', $v);
    }

    /**
     * @param array<mixed> $arr
     */
    private static function parseMailServiceToken(array $arr): string
    {
        $n = min(count($arr), 48);
        for ($i = 0; $i < $n; $i++) {
            $el = $arr[$i];
            if (is_array($el) && array_key_exists('mailServiceToken', $el)) {
                $t = self::resolveRef($arr, $el['mailServiceToken']);
                if (self::isJwt($t)) {
                    return $t;
                }
            }
        }
        foreach ($arr as $el) {
            if (is_array($el) && array_key_exists('mailServiceToken', $el)) {
                $t = self::resolveRef($arr, $el['mailServiceToken']);
                if (self::isJwt($t)) {
                    return $t;
                }
            }
        }
        foreach ($arr as $el) {
            if (self::isJwt($el)) {
                return $el;
            }
        }
        throw new \RuntimeException('10minute-one: 未找到 mailServiceToken');
    }

    /**
     * 从 HTML 中提取形如 field:"[...]" 的转义 JSON 数组。
     *
     * @return string[]
     */
    private static function parseQuotedJsonArray(string $html, string $field): array
    {
        $key = $field . ':"';
        $start = strpos($html, $key);
        if ($start === false) {
            return [];
        }
        $sliceStart = $start + strlen($key);
        if ($sliceStart >= strlen($html) || $html[$sliceStart] !== '[') {
            return [];
        }
        $depth = 0;
        $j = $sliceStart;
        $len = strlen($html);
        while ($j < $len) {
            $c = $html[$j];
            if ($c === '[') {
                $depth++;
            } elseif ($c === ']') {
                $depth--;
                if ($depth === 0) {
                    $j++;
                    break;
                }
            }
            $j++;
        }
        $raw = substr($html, $sliceStart, $j - $sliceStart);
        $unesc = str_replace('\\"', '"', $raw);
        $v = json_decode($unesc, true);
        if (!is_array($v)) {
            return [];
        }
        $out = [];
        foreach ($v as $s) {
            if (is_string($s)) {
                $out[] = $s;
            }
        }
        return $out;
    }

    private static function pickLocale(?string $domain): string
    {
        $s = trim((string) $domain);
        if ($s === '') {
            return 'zh';
        }
        if (str_contains($s, '.') && !str_contains($s, '/')) {
            return 'zh';
        }
        return $s;
    }

    /**
     * @param string[] $available
     */
    private static function pickMailboxDomain(?string $hint, array $available): string
    {
        if (empty($available)) {
            throw new \RuntimeException('10minute-one: 无可用邮箱域名');
        }
        if ($hint !== null) {
            $h = strtolower(trim($hint));
            if (str_contains($h, '.')) {
                foreach ($available as $d) {
                    if (strtolower($d) === $h) {
                        return $d;
                    }
                }
            }
        }
        return $available[random_int(0, count($available) - 1)];
    }

    private static function randomLocal(int $n): string
    {
        $alphabet = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $n; $i++) {
            $out .= $alphabet[random_int(0, strlen($alphabet) - 1)];
        }
        return $out;
    }

    /**
     * 解析 JWT payload 中的 exp（秒）并转为毫秒时间戳。
     */
    private static function jwtExpMs(string $token): ?int
    {
        $parts = explode('.', $token);
        if (count($parts) < 2) {
            return null;
        }
        $seg = strtr($parts[1], '-_', '+/');
        $seg .= str_repeat('=', (4 - strlen($seg) % 4) % 4);
        $decoded = base64_decode($seg, true);
        if ($decoded === false) {
            return null;
        }
        $payload = json_decode($decoded, true);
        if (is_array($payload) && isset($payload['exp']) && is_numeric($payload['exp'])) {
            return (int) ($payload['exp'] * 1000);
        }
        return null;
    }

    /**
     * @return array<string,string>
     */
    private static function apiHeaders(string $token): array
    {
        return [
            'Accept' => '*/*',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
            'Authorization' => 'Bearer ' . $token,
            'Cache-Control' => 'no-cache',
            'Content-Type' => 'application/json',
            'DNT' => '1',
            'Origin' => self::SITE_ORIGIN,
            'Pragma' => 'no-cache',
            'Referer' => self::SITE_ORIGIN . '/',
            'User-Agent' => self::DEFAULT_UA,
            'X-Request-ID' => bin2hex(random_bytes(16)),
            'X-Timestamp' => (string) time(),
        ];
    }

    /**
     * @param array<mixed> $m
     */
    private static function itemNeedsDetail(array $m): bool
    {
        if (trim((string) ($m['id'] ?? '')) === '') {
            return false;
        }
        $body = trim((string) ($m['text'] ?? $m['body'] ?? $m['html'] ?? $m['mail_text'] ?? ''));
        return $body === '';
    }

    public static function generate(?string $domain = null, string $channel = '10minute-one'): EmailInfo
    {
        $selectedChannel = trim($channel) ?: '10minute-one';
        $loc = self::pickLocale($domain);
        $pageUrl = self::SITE_ORIGIN . '/' . $loc;
        $resp = HttpClient::get($pageUrl, [
            'User-Agent' => self::DEFAULT_UA,
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
            'Cache-Control' => 'no-cache',
            'Pragma' => 'no-cache',
            'DNT' => '1',
            'Referer' => $pageUrl,
        ]);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('10minute-one: 页面请求失败');
        }
        $html = (string) $resp->getBody();
        $arr = self::parseNuxtArray($html);
        $token = self::parseMailServiceToken($arr);

        $domains = array_values(array_unique(array_merge(
            self::parseQuotedJsonArray($html, 'emailDomains'),
            self::KNOWN_EMAIL_DOMAINS,
        )));
        if (empty($domains)) {
            $domains = self::KNOWN_EMAIL_DOMAINS;
        }

        $blocked = [];
        foreach (self::parseQuotedJsonArray($html, 'blockedUsernames') as $u) {
            $blocked[strtolower($u)] = true;
        }

        $domHint = null;
        $trimmedDomain = trim((string) $domain);
        if ($trimmedDomain !== '' && str_contains($trimmedDomain, '.')) {
            $domHint = $trimmedDomain;
        }
        $mailDomain = self::pickMailboxDomain($domHint, $domains);

        $local = '';
        for ($i = 0; $i < 32; $i++) {
            $cand = self::randomLocal(10);
            if (!isset($blocked[strtolower($cand)])) {
                $local = $cand;
                break;
            }
        }
        if ($local === '') {
            throw new \RuntimeException('10minute-one: 无法生成用户名');
        }

        $address = $local . '@' . $mailDomain;
        return new EmailInfo($selectedChannel, $address, $token, expiresAt: self::jwtExpMs($token));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('10minute-one: token 不能为空');
        }
        $url = self::API_BASE . '/mailbox/' . self::encMailboxEmail($email);
        $resp = HttpClient::get($url, self::apiHeaders($token));
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('10minute-one: 拉取邮件失败');
        }
        $text = (string) $resp->getBody();
        $data = $text === '' ? null : json_decode($text, true);
        if (!is_array($data) || !array_is_list($data)) {
            throw new \RuntimeException('10minute-one: 收件箱 JSON 非法');
        }

        $out = [];
        foreach ($data as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $row = $raw;
            if (self::itemNeedsDetail($row)) {
                $mid = (string) ($row['id'] ?? '');
                try {
                    $du = self::API_BASE . '/mailbox/' . self::encMailboxEmail($email) . '/' . rawurlencode($mid);
                    $dr = HttpClient::get($du, self::apiHeaders($token));
                    if ($dr->getStatusCode() < 400) {
                        $detail = HttpClient::json($dr);
                        if (!empty($detail)) {
                            // setdefault 语义：仅填充列表数据缺失的字段
                            foreach ($detail as $k => $v) {
                                if (!array_key_exists($k, $row)) {
                                    $row[$k] = $v;
                                }
                            }
                        }
                    }
                } catch (\Throwable) {
                }
            }
            $out[] = Normalize::email($row, $email);
        }
        return $out;
    }
}
