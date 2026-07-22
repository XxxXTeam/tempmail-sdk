<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * 10minutemail.net 渠道实现（渠道标识 ten-minute-mail-net，基于 address.api.php JSON 接口）
 *
 * API: https://10minutemail.net
 * 流程: GET /address.api.php 创建邮箱并从 Set-Cookie 提取 PHPSESSID（token 序列化为 JSON）→
 *       GET /address.api.php（带 cookie）取 mail_list → GET /mail.api.php?mailid={id} 取详情。
 */
final class TenMinuteMailNetApi
{
    private const CHANNEL = 'ten-minute-mail-net';
    private const BASE_URL = 'https://10minutemail.net';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36',
        'Accept' => 'application/json',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL . '/address.api.php', self::HEADERS);
        $data = HttpClient::json($resp);
        $address = (string) ($data['mail_get_mail'] ?? '');
        if ($address === '') {
            throw new \RuntimeException('ten-minute-mail-net: 创建邮箱失败');
        }
        $phpsessid = self::extractPhpsessid($resp);
        if ($phpsessid === '') {
            throw new \RuntimeException('ten-minute-mail-net: 未获取到 PHPSESSID cookie');
        }
        $token = (string) json_encode(['cookie' => 'PHPSESSID=' . $phpsessid], JSON_UNESCAPED_SLASHES);

        $expiresAt = null;
        $duetime = $data['mail_get_duetime'] ?? null;
        if (is_numeric($duetime)) {
            $expiresAt = (int) $duetime * 1000;
        }

        return new EmailInfo(self::CHANNEL, $address, $token, expiresAt: $expiresAt);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('ten-minute-mail-net: token 不能为空');
        }
        $tokenData = json_decode($token, true);
        $cookieStr = is_array($tokenData) ? (string) ($tokenData['cookie'] ?? '') : '';
        $headers = self::HEADERS + ['Cookie' => $cookieStr];

        $resp = HttpClient::get(self::BASE_URL . '/address.api.php', $headers);
        $data = HttpClient::json($resp);
        $mailList = $data['mail_list'] ?? null;
        if (!is_array($mailList)) {
            return [];
        }

        $out = [];
        foreach ($mailList as $item) {
            if (!is_array($item)) {
                continue;
            }
            $mailId = (string) ($item['mail_id'] ?? '');
            // 排除系统欢迎邮件
            if ($mailId === '' || $mailId === 'welcome') {
                continue;
            }

            $detailResp = HttpClient::get(self::BASE_URL . '/mail.api.php?mailid=' . rawurlencode($mailId), $headers);
            $detail = HttpClient::json($detailResp);
            if (empty($detail)) {
                continue;
            }

            [$htmlBody, $textBody] = self::extractBodies($detail);
            $raw = [
                'id' => $mailId,
                'from' => ($detail['from'] ?? '') ?: ($item['from'] ?? ''),
                'to' => ($detail['to'] ?? '') ?: $email,
                'subject' => ($detail['subject'] ?? '') ?: ($item['subject'] ?? ''),
                'text' => $textBody,
                'html' => $htmlBody,
                'date' => ($detail['datetime'] ?? '') ?: ($item['datetime'] ?? ''),
            ];
            $out[] = Normalize::email($raw, $email);
        }
        return $out;
    }

    /**
     * 从邮件详情提取 HTML 与纯文本正文。
     *
     * @param array<mixed> $detail
     * @return array{0:string,1:string}
     */
    private static function extractBodies(array $detail): array
    {
        $htmlBody = '';
        $textBody = '';
        $bodyList = $detail['body'] ?? null;
        if (is_array($bodyList)) {
            foreach ($bodyList as $part) {
                if (!is_array($part)) {
                    continue;
                }
                $contentType = (string) ($part['content'] ?? '');
                if (str_contains($contentType, 'text/html')) {
                    $htmlBody = (string) ($part['body'] ?? '');
                } elseif (str_contains($contentType, 'text/plain')) {
                    $textBody = (string) ($part['body'] ?? '');
                }
            }
        }
        if ($textBody === '') {
            $plainList = $detail['plain'] ?? null;
            if (is_array($plainList) && isset($plainList[0]) && is_string($plainList[0])) {
                $textBody = $plainList[0];
            }
        }
        return [$htmlBody, $textBody];
    }

    private static function extractPhpsessid(ResponseInterface $resp): string
    {
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if (str_starts_with($first, 'PHPSESSID=')) {
                return substr($first, strlen('PHPSESSID='));
            }
        }
        return '';
    }
}
