<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * tempemail.info 渠道实现 — https://tempemail.info
 *
 * 创建: GET / 获取 PHPSESSID cookie，并从 HTML 提取 emailEncoded（base64）解码为邮箱地址；
 * 读信: POST /template/checker.php（body: last_id=0）取列表 → GET /view/{date} 取正文。
 * token 保存会话 Cookie 头。
 */
final class TempEmailInfo
{
    private const CHANNEL = 'tempemail-info';
    private const BASE_URL = 'https://tempemail.info';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * @return array<string,string>
     */
    private static function baseHeaders(): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
            'Referer' => self::BASE_URL . '/',
            'Origin' => self::BASE_URL,
        ];
    }

    public static function generate(): EmailInfo
    {
        $headers = self::baseHeaders();
        $headers['Accept'] = 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8';
        $resp = HttpClient::get(self::BASE_URL . '/', $headers);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('tempemail-info: 首页请求失败 ' . $resp->getStatusCode());
        }
        $cookieHdr = self::cookieHeader($resp);
        if ($cookieHdr === '') {
            throw new \RuntimeException('tempemail-info: 未获取到会话 Cookie');
        }
        if (preg_match('/var\s+emailEncoded\s*=\s*"([^"]+)"/', (string) $resp->getBody(), $m) !== 1) {
            throw new \RuntimeException('tempemail-info: 未在页面中找到 emailEncoded');
        }
        $decoded = base64_decode($m[1], true);
        $decoded = $decoded !== false ? trim($decoded) : '';
        if ($decoded === '' || !str_contains($decoded, '@')) {
            throw new \RuntimeException('tempemail-info: 解码出的邮箱地址无效');
        }
        return new EmailInfo(self::CHANNEL, $decoded, $cookieHdr);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $cookieHdr = trim((string) $token);
        if ($cookieHdr === '') {
            throw new \InvalidArgumentException('tempemail-info: 缺少会话 Cookie');
        }

        $headers = self::baseHeaders();
        $headers['Accept'] = 'application/json, text/javascript, */*; q=0.01';
        $headers['X-Requested-With'] = 'XMLHttpRequest';
        $headers['Content-Type'] = 'application/x-www-form-urlencoded';
        $headers['Cookie'] = $cookieHdr;

        $resp = HttpClient::post(self::BASE_URL . '/template/checker.php', $headers, form: ['last_id' => '0']);
        $rows = HttpClient::json($resp);
        if (!array_is_list($rows) || empty($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                continue;
            }
            $fromAddr = (string) ($row['from'] ?? '');
            $name = (string) ($row['name'] ?? '');
            if ($name !== '' && $name !== $fromAddr) {
                $fromAddr = $name . ' <' . $fromAddr . '>';
            }
            $date = (string) ($row['date'] ?? '');
            $out[] = Normalize::email([
                'id' => (string) ($row['id'] ?? ''),
                'from' => $fromAddr,
                'to' => $email,
                'subject' => $row['subject'] ?? '',
                'date' => $date,
                'html' => self::fetchBody($cookieHdr, $date),
                'isRead' => (bool) ($row['read'] ?? false),
            ], $email);
        }
        return $out;
    }

    /**
     * 获取单封邮件 HTML 正文（GET /view/{date}），失败返回空串。
     */
    private static function fetchBody(string $cookieHdr, string $date): string
    {
        if (trim($date) === '') {
            return '';
        }
        $headers = self::baseHeaders();
        $headers['Accept'] = 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8';
        $headers['Cookie'] = $cookieHdr;
        try {
            $resp = HttpClient::get(self::BASE_URL . '/view/' . rawurlencode($date), $headers);
        } catch (\Throwable) {
            return '';
        }
        $status = $resp->getStatusCode();
        if ($status < 200 || $status >= 300) {
            return '';
        }
        return (string) $resp->getBody();
    }

    private static function cookieHeader(ResponseInterface $resp): string
    {
        $d = [];
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first !== '' && str_contains($first, '=')) {
                [$k, $v] = explode('=', $first, 2);
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
}
