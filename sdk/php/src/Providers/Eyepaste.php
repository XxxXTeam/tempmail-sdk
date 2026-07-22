<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;

/**
 * EyePaste 渠道实现（https://eyepaste.com）
 *
 * RSS XML API，无认证：generate 直接随机用户名 + @eyepaste.com；
 * getEmails GET /inbox/{email}.rss 解析 RSS，item.description 首个 <p> 含元信息。
 */
final class Eyepaste
{
    private const DOMAIN = 'eyepaste.com';
    private const BASE = 'https://www.eyepaste.com';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/rss+xml, application/xml, text/xml, */*',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    private static function randomUsername(int $length = 10): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(): EmailInfo
    {
        $email = self::randomUsername() . '@' . self::DOMAIN;
        return new EmailInfo('eyepaste', $email);
    }

    /**
     * 解析 RSS description，返回 from/to/subject/date/body。
     *
     * @return array{from:string,to:string,subject:string,date:string,body:string}
     */
    private static function parseDescription(string $desc): array
    {
        $result = ['from' => '', 'to' => '', 'subject' => '', 'date' => '', 'body' => ''];
        if ($desc === '') {
            return $result;
        }
        if (preg_match('/<p[^>]*>(.*?)<\/p>/is', $desc, $pm) === 1) {
            $meta = trim($pm[1]);
            if (preg_match('/From:\s*(.*?)(?:\s+To:|$)/s', $meta, $m) === 1) {
                $result['from'] = trim($m[1]);
            }
            if (preg_match('/To:\s*(.*?)(?:\s+Subject:|$)/s', $meta, $m) === 1) {
                $result['to'] = trim($m[1]);
            }
            if (preg_match('/Subject:\s*(.*?)(?:\s+Date:|$)/s', $meta, $m) === 1) {
                $result['subject'] = trim($m[1]);
            }
            if (preg_match('/Date:\s*(.*?)$/s', $meta, $m) === 1) {
                $result['date'] = trim($m[1]);
            }
            $pEnd = strpos($desc, '</p>');
            if ($pEnd !== false) {
                $body = trim(substr($desc, $pEnd + 4));
                if ($body !== '') {
                    $result['body'] = $body;
                }
            }
        }
        return $result;
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $e = trim($email);
        if ($e === '') {
            throw new \InvalidArgumentException('eyepaste: empty email');
        }
        $resp = HttpClient::get(self::BASE . '/inbox/' . $e . '.rss', self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('eyepaste: RSS 请求失败 ' . $resp->getStatusCode());
        }
        $content = trim((string) $resp->getBody());
        if ($content === '') {
            return [];
        }
        $prev = libxml_use_internal_errors(true);
        $root = simplexml_load_string($content);
        libxml_use_internal_errors($prev);
        if ($root === false || !isset($root->channel)) {
            return [];
        }

        $out = [];
        $idx = 0;
        foreach ($root->channel->item as $item) {
            $idx++;
            $title = trim((string) $item->title);
            $desc = trim((string) $item->description);
            $parsed = self::parseDescription($desc);
            $subject = $parsed['subject'] !== '' ? $parsed['subject'] : $title;
            $bodyHtml = $parsed['body'];
            $text = $bodyHtml !== '' ? trim(preg_replace('/<[^>]+>/', '', $bodyHtml) ?? '') : '';
            $out[] = new Email(
                id: (string) $idx,
                fromAddr: $parsed['from'],
                to: $parsed['to'] !== '' ? $parsed['to'] : $e,
                subject: $subject,
                text: $text,
                html: $bodyHtml,
                date: $parsed['date'],
                isRead: false,
                attachments: [],
            );
        }
        return $out;
    }
}
