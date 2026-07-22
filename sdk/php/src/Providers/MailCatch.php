<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * MailCatch 渠道实现（https://mailcatch.com）
 *
 * 公开收件箱，无需认证：generate 随机用户名 + @mailcatch.com；
 * getEmails GET /api/list/{inbox} 拿列表 HTML，逐封 GET /api/data/{inbox}/{id} 取正文。
 */
final class MailCatch
{
    private const BASE_URL = 'https://mailcatch.com';
    private const DOMAIN = 'mailcatch.com';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = 'sdk';
        for ($i = 0; $i < 16; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(): EmailInfo
    {
        $local = self::randomLocal();
        $email = $local . '@' . self::DOMAIN;
        return new EmailInfo('mailcatch', $email, $local);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('mailcatch: token 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('mailcatch: 邮箱地址不能为空');
        }
        $inbox = trim($token);
        if ($inbox === '') {
            $inbox = trim(explode('@', $addr)[0]);
        }
        if ($inbox === '') {
            throw new \InvalidArgumentException('mailcatch: 无法确定收件箱名称');
        }

        $resp = HttpClient::get(self::BASE_URL . '/api/list/' . $inbox, self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('mailcatch: 列表请求失败 ' . $resp->getStatusCode());
        }
        $listHtml = (string) $resp->getBody();

        $pattern = '/class="email-item"\s+'
            . 'data-email-id="([^"]*)"\s+'
            . 'data-subject="([^"]*)"\s+'
            . 'data-timestamp="([^"]*)"\s+'
            . 'data-sender="([^"]*)"/is';
        if (preg_match_all($pattern, $listHtml, $items, PREG_SET_ORDER) === false || empty($items)) {
            return [];
        }

        $out = [];
        foreach ($items as $item) {
            $emailId = trim($item[1]);
            if ($emailId === '') {
                continue;
            }
            $bodyHtml = '';
            try {
                $detail = HttpClient::get(self::BASE_URL . '/api/data/' . $inbox . '/' . $emailId, self::HEADERS);
                if ($detail->getStatusCode() === 200) {
                    $bodyHtml = trim((string) $detail->getBody());
                }
            } catch (\Throwable) {
            }
            $out[] = Normalize::email([
                'id' => $emailId,
                'from' => trim($item[4]),
                'to' => $addr,
                'subject' => trim($item[2]),
                'html' => $bodyHtml,
                'date' => trim($item[3]),
            ], $addr);
        }
        return $out;
    }
}
