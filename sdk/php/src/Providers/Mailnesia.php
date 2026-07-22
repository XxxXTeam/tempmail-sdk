<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Mailnesia 渠道实现（https://mailnesia.com）
 *
 * 公开收件箱，无认证：generate 随机用户名 + @mailnesia.com（预热访问 mailbox 页面）；
 * getEmails GET /mailbox/{local} 解析 <tr class="emailheader"> 行，
 * 再逐封 GET /mailbox/{local}/{id} 提取正文（text_plain / text_html）。
 */
final class Mailnesia
{
    private const CHANNEL = 'mailnesia';
    private const BASE_URL = 'https://mailnesia.com';
    private const DOMAIN = 'mailnesia.com';

    /** @var array<string,string> */
    private const HEADERS = ['Accept' => 'text/html,*/*'];

    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = 'sdk';
        for ($i = 0; $i < 16; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    private static function localPart(string $email): string
    {
        return trim(explode('@', trim($email))[0]);
    }

    private static function mailboxUrl(string $local): string
    {
        return self::BASE_URL . '/mailbox/' . rawurlencode($local);
    }

    private static function detailUrl(string $local, string $messageId): string
    {
        return self::mailboxUrl($local) . '/' . rawurlencode($messageId);
    }

    private static function fetchHtml(string $url): string
    {
        $resp = HttpClient::get($url, self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('mailnesia: 请求失败 ' . $resp->getStatusCode());
        }
        return (string) $resp->getBody();
    }

    private static function cleanText(string $raw): string
    {
        $s = preg_replace('/<[^>]+>/s', ' ', $raw) ?? '';
        $s = html_entity_decode($s, ENT_QUOTES | ENT_HTML5);
        $parts = preg_split('/\s+/', trim($s)) ?: [];
        return implode(' ', array_filter($parts, static fn ($p) => $p !== ''));
    }

    /**
     * 解析收件箱页面表格行。
     *
     * @return array<int,array{id:string,date:string,from:string,to:string,subject:string}>
     */
    private static function parseRows(string $page): array
    {
        $rows = [];
        $rowRe = '/<tr\s+id="([^"]+)"[^>]*class="[^"]*\bemailheader\b[^"]*"[^>]*>(.*?)<\/tr>/is';
        if (preg_match_all($rowRe, $page, $matches, PREG_SET_ORDER) === false) {
            return [];
        }
        foreach ($matches as $m) {
            $messageId = trim($m[1]);
            $rowHtml = $m[2];
            $date = '';
            if (preg_match('/<time\s+datetime="([^"]+)"/is', $rowHtml, $dm) === 1) {
                $date = html_entity_decode(trim($dm[1]), ENT_QUOTES | ENT_HTML5);
            }
            $anchors = [];
            if (preg_match_all('/<a\b[^>]*class="email"[^>]*>(.*?)<\/a>/is', $rowHtml, $am) !== false) {
                foreach ($am[1] as $a) {
                    $anchors[] = self::cleanText($a);
                }
            }
            if (count($anchors) < 3) {
                continue;
            }
            $rows[] = [
                'id' => $messageId,
                'date' => $date,
                'from' => $anchors[0],
                'to' => $anchors[1],
                'subject' => $anchors[2],
            ];
        }
        return $rows;
    }

    private static function extractDivById(string $page, string $divId, string $nextId = ''): string
    {
        $needle = 'id="' . $divId . '"';
        $pos = strpos($page, $needle);
        if ($pos === false) {
            return '';
        }
        $openEnd = strpos($page, '>', $pos);
        if ($openEnd === false) {
            return '';
        }
        $start = $openEnd + 1;
        $end = false;
        if ($nextId !== '') {
            $end = strpos($page, '<div id="' . $nextId . '"', $start);
        }
        if ($end === false) {
            $close = strpos($page, '</div>', $start);
            if ($close !== false) {
                $end = $close + strlen('</div>');
            }
        }
        if ($end === false) {
            return '';
        }
        $content = trim(substr($page, $start, $end - $start));
        if (str_ends_with($content, '</div>')) {
            $content = trim(substr($content, 0, -strlen('</div>')));
        }
        return $content;
    }

    private static function extractPlain(string $page, string $messageId): string
    {
        $pattern = '/<div\s+id="text_plain_' . preg_quote($messageId, '/')
            . '"[^>]*>\s*<pre>(.*?)<\/pre>\s*<\/div>/is';
        if (preg_match($pattern, $page, $m) === 1) {
            return trim(html_entity_decode($m[1], ENT_QUOTES | ENT_HTML5));
        }
        return '';
    }

    /**
     * 获取单封邮件详情，补充 text/html。
     *
     * @param array{id:string,date:string,from:string,to:string,subject:string} $row
     * @return array<string,mixed>
     */
    private static function fetchDetail(string $local, array $row): array
    {
        $messageId = trim((string) ($row['id'] ?? ''));
        if ($messageId === '') {
            return $row;
        }
        $page = self::fetchHtml(self::detailUrl($local, $messageId));
        $detail = $row;
        $detail['text'] = self::extractPlain($page, $messageId);
        $detail['html'] = self::extractDivById(
            $page,
            'text_html_' . $messageId,
            'text_plain_' . $messageId,
        );
        return $detail;
    }

    public static function generate(): EmailInfo
    {
        $local = self::randomLocal();
        self::fetchHtml(self::mailboxUrl($local));
        return new EmailInfo(self::CHANNEL, $local . '@' . self::DOMAIN);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $local = self::localPart($email);
        if ($local === '') {
            throw new \InvalidArgumentException('mailnesia: empty email');
        }
        $page = self::fetchHtml(self::mailboxUrl($local));
        $rows = self::parseRows($page);
        $out = [];
        foreach ($rows as $row) {
            try {
                $out[] = Normalize::email(self::fetchDetail($local, $row), $email);
            } catch (\Throwable) {
                $out[] = Normalize::email($row, $email);
            }
        }
        return $out;
    }
}
