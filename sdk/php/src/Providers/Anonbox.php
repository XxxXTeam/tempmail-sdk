<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * anonbox.net (CCC) 渠道实现
 *
 * GET /en/ 解析 HTML 提取邮箱与 token（inbox/secret），
 * GET /{inbox}/{secret}/ 返回 mbox 文本，按 "From " 分块解析每封邮件。
 */
final class Anonbox
{
    private const PAGE_URL = 'https://anonbox.net/en/';
    private const BASE = 'https://anonbox.net';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
        'Accept' => 'text/html,application/xhtml+xml',
    ];

    /**
     * 去除 HTML 标签并反转义常见实体。
     */
    private static function stripTags(string $html): string
    {
        $s = preg_replace('/<[^>]+>/', '', $html) ?? '';
        $s = str_replace(['&nbsp;', '&amp;', '&lt;', '&gt;'], [' ', '&', '<', '>'], $s);
        return trim($s);
    }

    /**
     * 简单 31 进制哈希，用于缺失 Message-ID 时生成稳定 ID（对齐 Python 实现）。
     */
    private static function simpleHash(string $s): string
    {
        $h = 0;
        $len = strlen($s);
        for ($i = 0; $i < $len; $i++) {
            $h = ($h * 31 + ord($s[$i])) & 0xFFFFFFFF;
        }
        $n = $h;
        if ($n === 0) {
            return '0';
        }
        $digits = '0123456789abcdefghijklmnopqrstuvwxyz';
        $out = '';
        while ($n > 0) {
            $out = $digits[$n % 36] . $out;
            $n = intdiv($n, 36);
        }
        return $out;
    }

    /**
     * 解析 /en/ 页面，返回 [email, token, expires]。
     *
     * @return array{0:string,1:string,2:?string}
     */
    private static function parseEnPage(string $html): array
    {
        if (
            preg_match(
                '#<a href="https://anonbox\.net/([a-z0-9-]+)/([A-Za-z0-9._~-]+)">https://anonbox\.net/[^"]+</a>#i',
                $html,
                $m,
            ) !== 1
        ) {
            throw new \RuntimeException('anonbox: mailbox link not found');
        }
        $inbox = $m[1];
        $secret = $m[2];
        $token = $inbox . '/' . $secret;

        $addressDisplay = null;
        if (preg_match_all('#<dd([^>]*)>([\s\S]*?)</dd>#i', $html, $dds, PREG_SET_ORDER)) {
            $expectedDomain = strtolower($inbox . '.anonbox.net');
            foreach ($dds as $dd) {
                if (preg_match('/display\s*:\s*none/i', $dd[1]) === 1) {
                    continue;
                }
                if (preg_match('#<p>([\s\S]*?)</p>#i', $dd[2], $pm) !== 1) {
                    continue;
                }
                $pInner = preg_replace('#<span\b[^>]*>[\s\S]*?</span>#i', '', $pm[1]) ?? $pm[1];
                $display = self::stripTags($pInner);
                if (!str_contains($display, '@')) {
                    continue;
                }
                $at = strrpos($display, '@');
                $local = trim(substr($display, 0, $at));
                $domain = strtolower(trim(substr($display, $at + 1)));
                if ($domain === 'googlemail.com' || $domain !== $expectedDomain || $local === '') {
                    continue;
                }
                $addressDisplay = $display;
                break;
            }
        }
        if ($addressDisplay === null) {
            throw new \RuntimeException('anonbox: address paragraph not found');
        }
        $at = strpos($addressDisplay, '@');
        $local = trim(substr($addressDisplay, 0, $at));
        if ($local === '') {
            throw new \RuntimeException('anonbox: empty local part');
        }
        $email = $local . '@' . $inbox . '.anonbox.net';

        $exp = null;
        if (preg_match('#Your mail address is valid until:</dt>\s*<dd><p>([^<]+)</p>#is', $html, $em) === 1) {
            $exp = trim($em[1]);
        }
        return [$email, $token, $exp];
    }

    /**
     * 按需解码 quoted-printable 正文。
     */
    private static function decodeQpIfNeeded(string $body, string $headerBlock): string
    {
        $enc = '';
        if (preg_match('/^content-transfer-encoding:\s*([^\s]+)/im', $headerBlock, $m) === 1) {
            $enc = strtolower(trim($m[1]));
        }
        if ($enc !== 'quoted-printable') {
            return rtrim($body, "\r\n");
        }
        return rtrim(quoted_printable_decode($body), "\r\n");
    }

    /**
     * 将单个 mbox 块解析为标准邮件原始字段。
     *
     * @return array<mixed>
     */
    private static function mboxBlockToRaw(string $block, string $recipient): array
    {
        $normalized = str_replace("\r\n", "\n", $block);
        $lines = explode("\n", $normalized);
        $i = 0;
        if (!empty($lines) && str_starts_with($lines[0], 'From ')) {
            $i = 1;
        }
        $headers = [];
        $curKey = '';
        $n = count($lines);
        while ($i < $n) {
            $line = $lines[$i];
            if ($line === '') {
                $i++;
                break;
            }
            if (($line[0] === ' ' || $line[0] === "\t") && $curKey !== '') {
                $headers[$curKey] .= ' ' . trim($line);
            } else {
                $idx = strpos($line, ':');
                if ($idx !== false && $idx > 0) {
                    $curKey = strtolower(trim(substr($line, 0, $idx)));
                    $headers[$curKey] = trim(substr($line, $idx + 1));
                }
            }
            $i++;
        }
        $body = implode("\n", array_slice($lines, $i));
        $ct = strtolower($headers['content-type'] ?? '');
        $text = '';
        $html = '';
        if (str_contains($ct, 'multipart/')) {
            if (preg_match('/boundary="?([^";\s]+)"?/i', $headers['content-type'] ?? '', $bm) === 1) {
                $boundary = preg_quote($bm[1], '#');
                $parts = preg_split('#\r?\n--' . $boundary . '(?:--)?\r?\n#', $body) ?: [];
                foreach ($parts as $part) {
                    $part = trim($part);
                    if ($part === '' || $part === '--') {
                        continue;
                    }
                    $sep = strpos($part, "\n\n");
                    if ($sep === false) {
                        continue;
                    }
                    $ph = substr($part, 0, $sep);
                    $pb = substr($part, $sep + 2);
                    $pct = '';
                    if (preg_match('/^content-type:\s*([^\s;]+)/im', $ph, $pctm) === 1) {
                        $pct = strtolower($pctm[1]);
                    }
                    if ($pct === 'text/plain') {
                        $text = self::decodeQpIfNeeded($pb, $ph);
                    } elseif ($pct === 'text/html') {
                        $html = self::decodeQpIfNeeded($pb, $ph);
                    }
                }
            }
        }
        if ($text === '' && $html === '') {
            if (str_contains($ct, 'text/html')) {
                $html = self::decodeQpIfNeeded($body, $headers['content-type'] ?? '');
            } else {
                $text = self::decodeQpIfNeeded($body, $headers['content-type'] ?? '');
            }
        }
        $dateStr = $headers['date'] ?? '';
        $msgId = $headers['message-id'] ?? self::simpleHash(substr($block, 0, 512));
        return [
            'id' => $msgId,
            'from' => $headers['from'] ?? '',
            'to' => $headers['to'] ?? $recipient,
            'subject' => $headers['subject'] ?? '',
            'text' => $text,
            'html' => $html,
            'date' => $dateStr,
            'isRead' => false,
            'attachments' => [],
        ];
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::PAGE_URL, self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('anonbox: 首页请求失败 ' . $resp->getStatusCode());
        }
        [$email, $token, $exp] = self::parseEnPage((string) $resp->getBody());
        return new EmailInfo('anonbox', $email, $token, createdAt: $exp);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('anonbox: token 不能为空');
        }
        $path = str_ends_with($token, '/') ? $token : $token . '/';
        $resp = HttpClient::get(self::BASE . '/' . $path, [
            'User-Agent' => self::HEADERS['User-Agent'],
            'Accept' => 'text/plain,*/*',
        ]);
        if ($resp->getStatusCode() === 404) {
            return [];
        }
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('anonbox: 取信失败 ' . $resp->getStatusCode());
        }
        $raw = trim((string) $resp->getBody());
        if ($raw === '') {
            return [];
        }
        $blocks = preg_split('/\r?\n(?=From )/', $raw) ?: [];
        $out = [];
        foreach ($blocks as $b) {
            $b = trim($b);
            if ($b === '') {
                continue;
            }
            $out[] = Normalize::email(self::mboxBlockToRaw($b, $email), $email);
        }
        return $out;
    }
}
