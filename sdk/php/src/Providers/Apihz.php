<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\ConfigStore;
use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * apihz（接口盒子）渠道实现 — https://apihz.cn
 *
 * 需 id/key 凭据，内置公共账号 88888888/88888888（共享频次），
 * 可用配置 apihzId/apihzKey 或环境变量 APIHZ_ID/APIHZ_KEY 覆盖。
 *   GET /api/mail/mailcache.php 创建邮箱（有效期 10 分钟）；
 *   GET /api/mail/mailgetlist.php 读信（须携带创建时的 pwd）。
 * token 编码: "apihz1:" + base64(JSON{mail,pwd})。
 */
final class Apihz
{
    private const BASE_URL = 'https://cn.apihz.cn';
    private const TOK_PREFIX = 'apihz1:';
    private const PUBLIC_ID = '88888888';
    private const PUBLIC_KEY = '88888888';

    /** @var string[] */
    private const DOMAINS = ['apimail.email', 'apimail.vip'];

    private const DEFAULT_UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36';

    private static function userAgent(): string
    {
        foreach (ConfigStore::get()->headers as $k => $v) {
            if (strtolower((string) $k) === 'user-agent' && $v !== '') {
                return (string) $v;
            }
        }
        return self::DEFAULT_UA;
    }

    /**
     * 读取 apihz 调用凭据：优先配置，回退官方公共账号。
     *
     * @return array{0:string,1:string}
     */
    private static function credentials(): array
    {
        $cfg = ConfigStore::get();
        $cid = trim((string) ($cfg->apihzId ?? '')) ?: self::PUBLIC_ID;
        $ckey = trim((string) ($cfg->apihzKey ?? '')) ?: self::PUBLIC_KEY;
        return [$cid, $ckey];
    }

    private static function randomLocal(int $n): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $n; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    private static function randomPassword(): string
    {
        $chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < 12; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    private static function encToken(string $mail, string $pwd): string
    {
        $raw = json_encode(['mail' => $mail, 'pwd' => $pwd], JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
        return self::TOK_PREFIX . base64_encode((string) $raw);
    }

    /**
     * @return array{0:string,1:string}
     */
    private static function decToken(?string $tok): array
    {
        if ($tok === null || !str_starts_with($tok, self::TOK_PREFIX)) {
            throw new \InvalidArgumentException('apihz: 无效 token');
        }
        $decoded = base64_decode(substr($tok, strlen(self::TOK_PREFIX)), true);
        if ($decoded === false) {
            throw new \InvalidArgumentException('apihz: 无效 token');
        }
        $obj = json_decode($decoded, true);
        if (!is_array($obj)) {
            throw new \InvalidArgumentException('apihz: 无效 token');
        }
        $mail = trim((string) ($obj['mail'] ?? ''));
        $pwd = trim((string) ($obj['pwd'] ?? ''));
        if ($mail === '' || $pwd === '') {
            throw new \InvalidArgumentException('apihz: 无效 token');
        }
        return [$mail, $pwd];
    }

    /**
     * @param array<mixed> $msg
     * @param string[]     $keys
     */
    private static function first(array $msg, array $keys): string
    {
        foreach ($keys as $key) {
            $val = $msg[$key] ?? null;
            if ($val !== null && $val !== '') {
                return (string) $val;
            }
        }
        return '';
    }

    public static function generate(): EmailInfo
    {
        [$cid, $ckey] = self::credentials();
        $domain = self::DOMAINS[array_rand(self::DOMAINS)];
        $name = self::randomLocal(10);
        $pwd = self::randomPassword();

        $resp = HttpClient::get(
            self::BASE_URL . '/api/mail/mailcache.php',
            ['User-Agent' => self::userAgent(), 'Accept' => 'application/json'],
            query: [
                'id' => $cid,
                'key' => $ckey,
                'domain' => $domain,
                'name' => $name,
                'pwd' => $pwd,
                'buytype' => '0',
            ],
        );
        $data = HttpClient::json($resp);
        if (($data['code'] ?? null) !== 200 || empty($data['mail'])) {
            throw new \RuntimeException('apihz: 创建邮箱失败');
        }
        $mail = (string) $data['mail'];
        $finalPwd = (string) ($data['pwd'] ?? $pwd);
        return new EmailInfo('apihz', $mail, self::encToken($mail, $finalPwd));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(?string $token): array
    {
        [$mail, $pwd] = self::decToken($token);
        [$cid, $ckey] = self::credentials();

        $resp = HttpClient::get(
            self::BASE_URL . '/api/mail/mailgetlist.php',
            ['User-Agent' => self::userAgent(), 'Accept' => 'application/json'],
            query: ['id' => $cid, 'key' => $ckey, 'mail' => $mail, 'pwd' => $pwd, 'page' => '1'],
        );
        $data = HttpClient::json($resp);
        if (($data['code'] ?? null) !== 200) {
            return [];
        }
        $items = $data['data'] ?? null;
        if (!is_array($items)) {
            return [];
        }

        $out = [];
        foreach ($items as $msg) {
            if (!is_array($msg)) {
                continue;
            }
            $raw = [
                'id' => self::first($msg, ['time1']),
                'from' => self::first($msg, ['frommail', 'fromname']),
                'to' => $mail,
                'subject' => self::first($msg, ['subject']),
                'text' => self::first($msg, ['content']),
                'html' => self::first($msg, ['content']),
                'date' => self::first($msg, ['time2', 'time1']),
            ];
            $out[] = Normalize::email($raw, $mail);
        }
        return $out;
    }
}
