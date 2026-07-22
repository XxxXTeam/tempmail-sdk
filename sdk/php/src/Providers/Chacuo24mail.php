<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * 24mail.chacuo.net 渠道实现 — http://24mail.chacuo.net
 *
 * POST / form: data={email}&type=refresh&arg= 创建/刷新收件箱（token 存邮箱地址本身）；
 * 同一接口返回 data[0].list 邮件数组，字段 MID/FROM/SUBJECT/CONTENT/TIME。
 */
final class Chacuo24mail
{
    private const BASE_URL = 'http://24mail.chacuo.net/';
    private const DEFAULT_DOMAIN = 'chacuo.net';

    /** @var string[] */
    private const DOMAINS = ['chacuo.net', '027168.com'];

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = 'sdk';
        for ($i = 0; $i < 12; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    private static function pickDomain(?string $preferred): string
    {
        $p = strtolower(ltrim(trim((string) $preferred), '@'));
        if ($p !== '') {
            foreach (self::DOMAINS as $domain) {
                if (strtolower($domain) === $p) {
                    return $domain;
                }
            }
        }
        return self::DEFAULT_DOMAIN;
    }

    /**
     * @return array<string,string>
     */
    private static function headers(): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => 'application/json, text/javascript, */*; q=0.01',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
            'X-Requested-With' => 'XMLHttpRequest',
            'Content-Type' => 'application/x-www-form-urlencoded; charset=UTF-8',
            'Origin' => 'http://24mail.chacuo.net',
            'Referer' => 'http://24mail.chacuo.net/',
        ];
    }

    /**
     * POST /  data={email}&type=refresh&arg=，返回 JSON 字典。
     *
     * @return array<mixed>
     */
    private static function refresh(string $email): array
    {
        $resp = HttpClient::post(
            self::BASE_URL,
            self::headers(),
            form: ['data' => $email, 'type' => 'refresh', 'arg' => ''],
        );
        $result = HttpClient::json($resp);
        if (empty($result)) {
            throw new \RuntimeException('24mail-chacuo: 响应格式异常');
        }
        return $result;
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        $email = self::randomLocal() . '@' . self::pickDomain($domain);
        $result = self::refresh($email);
        if (($result['status'] ?? null) != 1) {
            throw new \RuntimeException('24mail-chacuo: 创建邮箱失败');
        }
        return new EmailInfo('24mail-chacuo', $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $addr = trim($email !== '' ? $email : (string) $token);
        if ($addr === '') {
            throw new \InvalidArgumentException('24mail-chacuo: 邮箱地址为空');
        }

        $result = self::refresh($addr);
        $data = $result['data'] ?? null;
        if (!is_array($data) || !array_is_list($data) || empty($data)) {
            return [];
        }
        $first = $data[0];
        $rows = is_array($first) ? ($first['list'] ?? null) : null;
        if (!is_array($rows) || empty($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $raw = [
                'id' => $item['MID'] ?? '',
                'from' => $item['FROM'] ?? '',
                'to' => $addr,
                'subject' => $item['SUBJECT'] ?? '',
                'content' => $item['CONTENT'] ?? '',
                'date' => $item['TIME'] ?? '',
            ];
            $out[] = Normalize::email($raw, $addr);
        }
        return $out;
    }
}
