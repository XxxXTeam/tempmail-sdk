<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * linshiyouxiang.net 渠道实现 — https://www.linshiyouxiang.net
 *
 * GET / 首页 HTML 中正则提取 tempMailGlobal（邮箱）与 mailCodeGlobal（校验 code）；
 * token 存储 code（HMAC 哈希，无需 cookie）；
 * POST /get-messages body: {"email":...,"code":...} 拉取邮件。
 */
final class LinshiyouxiangNet
{
    private const BASE_URL = 'https://www.linshiyouxiang.net';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * @return array<string,string>
     */
    private static function apiHeaders(): array
    {
        return [
            'Accept' => 'application/json, text/javascript, */*; q=0.01',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
            'Referer' => self::BASE_URL . '/',
            'Origin' => self::BASE_URL,
            'User-Agent' => self::UA,
            'Content-Type' => 'application/json',
            'X-Requested-With' => 'XMLHttpRequest',
        ];
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL . '/', [
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
            'User-Agent' => self::UA,
        ]);
        $html = (string) $resp->getBody();

        if (preg_match("/tempMailGlobal\\s*=\\s*'([^']+)'/", $html, $m) !== 1) {
            throw new \RuntimeException('linshiyouxiang-net: 未能从首页提取邮箱地址');
        }
        $email = trim($m[1]);
        if ($email === '') {
            throw new \RuntimeException('linshiyouxiang-net: 提取的邮箱地址为空');
        }

        $code = '';
        if (preg_match("/mailCodeGlobal\\s*=\\s*'([^']+)'/", $html, $cm) === 1) {
            $code = trim($cm[1]);
        }

        return new EmailInfo('linshiyouxiang-net', $email, $code);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('linshiyouxiang-net: 邮箱地址为空');
        }

        $resp = HttpClient::post(
            self::BASE_URL . '/get-messages',
            self::apiHeaders(),
            json: ['email' => $address, 'code' => (string) $token],
        );
        $result = HttpClient::json($resp);
        $emails = $result['emails'] ?? null;
        if (!is_array($emails)) {
            return [];
        }

        $out = [];
        foreach ($emails as $item) {
            if (is_array($item)) {
                $out[] = Normalize::email($item, $address);
            }
        }
        return $out;
    }
}
