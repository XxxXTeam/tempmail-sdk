<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\ConfigStore;
use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * tempemail.co 渠道实现 — https://tempemail.co
 *
 * Cookie Session + REST JSON。
 * 创建: GET /mail/random 取随机邮箱并保存 session cookie（token 序列化为 JSON）；
 * 读信: GET /get-mails 取列表 HTML → 正则提取 data-id → GET /mail/info?id={id} 取详情。
 */
final class TempEmailCo
{
    private const CHANNEL = 'tempemail-co';
    private const BASE_URL = 'https://tempemail.co';

    private const DEFAULT_UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    private static function ua(): string
    {
        foreach (ConfigStore::get()->headers as $k => $v) {
            if (strtolower((string) $k) === 'user-agent' && $v !== '') {
                return (string) $v;
            }
        }
        return self::DEFAULT_UA;
    }

    /**
     * @return array<string,string>
     */
    private static function buildHeaders(string $referer = '', string $cookie = '', string $accept = ''): array
    {
        $h = [
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'en-US,en;q=0.9',
            'Cache-Control' => 'no-cache',
            'DNT' => '1',
            'Pragma' => 'no-cache',
            'Upgrade-Insecure-Requests' => '1',
            'User-Agent' => self::ua(),
        ];
        if ($referer !== '') {
            $h['Referer'] = $referer;
        }
        if ($cookie !== '') {
            $h['Cookie'] = $cookie;
        }
        if ($accept !== '') {
            $h['Accept'] = $accept;
        }
        return $h;
    }

    public static function generate(): EmailInfo
    {
        $r = HttpClient::get(
            self::BASE_URL . '/mail/random',
            self::buildHeaders(
                referer: self::BASE_URL . '/',
                accept: 'application/json, text/javascript, */*; q=0.01',
            ),
        );
        $data = HttpClient::json($r);
        if (empty($data['result'])) {
            throw new \RuntimeException('tempemail-co: 创建邮箱失败');
        }
        $address = trim((string) (($data['address'] ?? '') ?: ($data['id'] ?? '')));
        if ($address === '' || !str_contains($address, '@')) {
            throw new \RuntimeException('tempemail-co: 返回的邮箱地址无效');
        }
        $cookies = self::collectCookies($r);
        return new EmailInfo(self::CHANNEL, $address, self::encodeToken($address, $cookies));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('tempemail-co: token 不能为空');
        }
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('tempemail-co: 邮箱地址不能为空');
        }
        [$address, $cookies] = self::decodeToken($token);
        $cookieStr = self::cookiesToStr($cookies);

        $r = HttpClient::get(
            self::BASE_URL . '/get-mails',
            self::buildHeaders(
                referer: self::BASE_URL . '/',
                cookie: $cookieStr,
                accept: 'application/json, text/javascript, */*; q=0.01',
            ),
            query: ['mail_id' => $address, 'unseen' => '0', 'is_new' => '1'],
        );
        $data = HttpClient::json($r);
        $count = $data['count'] ?? 0;
        if (!is_numeric($count) || (int) $count <= 0) {
            return [];
        }
        $mailsHtml = $data['mails'] ?? '';
        if (!is_string($mailsHtml) || $mailsHtml === '') {
            return [];
        }
        if (preg_match_all('/data-id="(\d+)"/', $mailsHtml, $ms) === false || empty($ms[1])) {
            return [];
        }
        $uniqueIds = array_values(array_unique($ms[1]));

        $out = [];
        foreach ($uniqueIds as $mailId) {
            try {
                $detailResp = HttpClient::get(
                    self::BASE_URL . '/mail/info',
                    self::buildHeaders(
                        referer: self::BASE_URL . '/',
                        cookie: $cookieStr,
                        accept: 'application/json, text/javascript, */*; q=0.01',
                    ),
                    query: ['id' => $mailId],
                );
                if ($detailResp->getStatusCode() !== 200) {
                    continue;
                }
                $detailData = HttpClient::json($detailResp);
                if (empty($detailData['result'])) {
                    continue;
                }
                $mailInfo = $detailData['mail'] ?? null;
                if (!is_array($mailInfo)) {
                    continue;
                }
                $fromName = trim((string) ($mailInfo['fromName'] ?? ''));
                $fromAddr = trim((string) ($mailInfo['fromAddress'] ?? ''));
                $fromDisplay = $fromName !== '' ? $fromName . ' <' . $fromAddr . '>' : $fromAddr;

                $out[] = Normalize::email([
                    'id' => (string) $mailId,
                    'from' => $fromDisplay,
                    'from_address' => $fromAddr,
                    'from_name' => $fromName,
                    'to' => $addr,
                    'subject' => $mailInfo['subject'] ?? '',
                    'html' => $mailInfo['textHtml'] ?? '',
                    'date' => $mailInfo['displayDate'] ?? '',
                ], $addr);
            } catch (\Throwable) {
                continue;
            }
        }
        return $out;
    }

    /**
     * 收集响应 Set-Cookie 为映射。
     *
     * @return array<string,string>
     */
    private static function collectCookies(ResponseInterface $resp): array
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
        return $d;
    }

    /**
     * @param array<string,string> $cookies
     */
    private static function cookiesToStr(array $cookies): string
    {
        ksort($cookies);
        $parts = [];
        foreach ($cookies as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }

    /**
     * @param array<string,string> $cookies
     */
    private static function encodeToken(string $address, array $cookies): string
    {
        return (string) json_encode(['address' => $address, 'cookies' => $cookies], JSON_UNESCAPED_SLASHES);
    }

    /**
     * @return array{0:string,1:array<string,string>}
     */
    private static function decodeToken(string $token): array
    {
        $data = json_decode($token, true);
        if (!is_array($data)) {
            throw new \InvalidArgumentException('tempemail-co: token 格式无效');
        }
        $address = (string) ($data['address'] ?? '');
        $cookies = $data['cookies'] ?? null;
        if ($address === '' || !is_array($cookies)) {
            throw new \InvalidArgumentException('tempemail-co: token 数据不完整');
        }
        return [$address, $cookies];
    }
}
