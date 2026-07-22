<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailAttachment;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;

/**
 * linshiyou.com 渠道实现（临时邮）
 *
 * GET /api/user?user 返回响应体即邮箱地址，Set-Cookie 含 NEXUS_TOKEN；
 * token = "NEXUS_TOKEN=...; tmail-emails=email"；
 * getEmails GET /api/mail?unseen=1 返回 HTML 分片，以自定义分隔符切分邮件。
 */
final class Linshiyou
{
    private const ORIGIN = 'https://linshiyou.com';

    private const SEG_SPLIT = '<-----TMAILNEXTMAIL----->';
    private const SEG_CHOP = '<-----TMAILCHOPPER----->';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
        'accept-language' => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
        'cache-control' => 'no-cache',
        'dnt' => '1',
        'pragma' => 'no-cache',
        'priority' => 'u=1, i',
        'referer' => self::ORIGIN . '/',
        'sec-ch-ua' => '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
        'sec-ch-ua-mobile' => '?0',
        'sec-ch-ua-platform' => '"Windows"',
        'sec-fetch-dest' => 'empty',
        'sec-fetch-mode' => 'cors',
        'sec-fetch-site' => 'same-origin',
    ];

    private static function stripTags(string $s): string
    {
        $t = preg_replace('/<[^>]+>/', ' ', $s) ?? '';
        $t = html_entity_decode($t, ENT_QUOTES | ENT_HTML5);
        return implode(' ', preg_split('/\s+/', trim($t)) ?: []);
    }

    /**
     * 在 HTML 分片中提取指定 class 的 div 文本（class 需精确匹配）。
     */
    private static function pickDiv(string $fragment, string $className): string
    {
        if (preg_match_all('/<div class="([^"]+)"[^>]*>([\s\S]*?)<\/div>/i', $fragment, $ms, PREG_SET_ORDER)) {
            foreach ($ms as $m) {
                if ($m[1] === $className) {
                    return trim(self::stripTags($m[2]));
                }
            }
        }
        return '';
    }

    private static function parseDate(string $s): string
    {
        $s = trim($s);
        if ($s === '') {
            return '';
        }
        $ts = strtotime($s);
        if ($ts !== false) {
            return gmdate('Y-m-d\TH:i:s\Z', $ts);
        }
        return '';
    }

    private static function extractSrcdoc(string $bodyPart): string
    {
        if (preg_match('/\ssrcdoc="([^"]*)"/i', $bodyPart, $m) === 1) {
            return html_entity_decode($m[1], ENT_QUOTES | ENT_HTML5);
        }
        return '';
    }

    /**
     * @return Email[]
     */
    private static function parseSegments(string $raw, string $recipient): array
    {
        $raw = trim($raw);
        if ($raw === '') {
            return [];
        }
        $out = [];
        foreach (explode(self::SEG_SPLIT, $raw) as $seg) {
            $seg = trim($seg);
            if ($seg === '') {
                continue;
            }
            $parts = explode(self::SEG_CHOP, $seg, 2);
            $listPart = $parts[0];
            $bodyPart = $parts[1] ?? '';

            if (preg_match('/id="tmail-email-list-([a-f0-9]+)"/i', $listPart, $mid) !== 1) {
                continue;
            }
            $midStr = $mid[1];

            $fromList = self::pickDiv($listPart, 'name');
            $subjectList = self::pickDiv($listPart, 'subject');
            $previewList = self::pickDiv($listPart, 'body');

            $fromBody = self::pickDiv($bodyPart, 'tmail-email-sender');
            $timeBody = self::pickDiv($bodyPart, 'tmail-email-time');
            $titleBody = self::pickDiv($bodyPart, 'tmail-email-title');
            $htmlBody = self::extractSrcdoc($bodyPart);

            $fromAddr = $fromBody !== '' ? $fromBody : $fromList;
            $subject = $titleBody !== '' ? $titleBody : $subjectList;
            $text = $previewList !== '' ? $previewList : self::stripTags($htmlBody);

            $attachments = [];
            if (preg_match('#href="(/api/download\?id=[^"]+)"#i', $bodyPart, $dm) === 1) {
                $attachments[] = new EmailAttachment(filename: '', url: self::ORIGIN . $dm[1]);
            }

            $out[] = new Email(
                id: $midStr,
                fromAddr: $fromAddr,
                to: $recipient,
                subject: $subject,
                text: $text,
                html: $htmlBody,
                date: self::parseDate($timeBody),
                isRead: false,
                attachments: $attachments,
            );
        }
        return $out;
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::ORIGIN . '/api/user?user', self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('linshiyou: 请求失败 ' . $resp->getStatusCode());
        }
        $nexus = '';
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            if (preg_match('/NEXUS_TOKEN=([^;]+)/', $sc, $m) === 1) {
                $nexus = $m[1];
                break;
            }
        }
        if ($nexus === '') {
            throw new \RuntimeException('linshiyou: NEXUS_TOKEN not in Set-Cookie');
        }
        $email = trim((string) $resp->getBody());
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('linshiyou: invalid email in response body');
        }
        $token = 'NEXUS_TOKEN=' . $nexus . '; tmail-emails=' . $email;
        return new EmailInfo('linshiyou', $email, $token);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('linshiyou: token 不能为空');
        }
        $headers = self::HEADERS;
        $headers['Cookie'] = $token;
        $headers['x-requested-with'] = 'XMLHttpRequest';
        $resp = HttpClient::get(self::ORIGIN . '/api/mail?unseen=1', $headers);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('linshiyou: 读信失败 ' . $resp->getStatusCode());
        }
        return self::parseSegments((string) $resp->getBody(), $email);
    }
}
