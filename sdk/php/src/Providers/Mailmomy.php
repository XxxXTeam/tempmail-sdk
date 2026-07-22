<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * mailmomy 渠道实现（含全部固定域名变体）
 *
 * API: https://mailmomy.com
 * 免注册、无鉴权、无 CAPTCHA 的纯 GET JSON API 临时邮箱，Token 即邮箱地址本身。
 * 流程:
 *   1. 主渠道 mailmomy 通过 GET /api/domains/active 随机选域名（失败回退 mailmomy.com）；
 *      域名变体渠道则固定使用各自域名。
 *   2. 本地随机 10 位 [a-z0-9] 前缀，拼接为 <local>@<域名>。
 *   3. GET /api/mail/messages?to=<email>&page=1&limit=20 → {"emails": [...], ...}。
 * 全部变体共用同一套后端 API，仅生成域名不同，通过 makeGenerate/makeGetEmails 复用逻辑。
 */
final class Mailmomy
{
    private const BASE_URL = 'https://mailmomy.com';
    private const DEFAULT_DOMAIN = 'mailmomy.com';

    private const USER_AGENT = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36';

    /**
     * 构造带 UA 的 JSON 请求头。
     *
     * @return array<string,string>
     */
    private static function headers(): array
    {
        return [
            'User-Agent' => self::USER_AGENT,
            'Accept' => 'application/json',
        ];
    }

    /**
     * 生成 10 位小写字母数字随机邮箱前缀。
     */
    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < 10; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * 拉取 mailmomy 当前可用域名池并随机选取一个。
     *
     * GET /api/domains/active → JSON 字符串数组；请求或解析失败时回退到默认 mailmomy.com。
     */
    private static function pickDomain(): string
    {
        try {
            $resp = HttpClient::get(self::BASE_URL . '/api/domains/active', self::headers());
            if ($resp->getStatusCode() >= 400) {
                return self::DEFAULT_DOMAIN;
            }
            $data = HttpClient::json($resp);
        } catch (\Throwable) {
            return self::DEFAULT_DOMAIN;
        }

        $domains = [];
        foreach ($data as $d) {
            if (is_string($d) && $d !== '') {
                $domains[] = $d;
            }
        }
        if (empty($domains)) {
            return self::DEFAULT_DOMAIN;
        }
        return $domains[random_int(0, count($domains) - 1)];
    }

    /**
     * 生成指定渠道的 generate 闭包。
     *
     * @param string|null $fixedDomain 固定域名（域名变体渠道）；为 null 时从 API 随机选取域名
     * @return callable():EmailInfo
     */
    public static function makeGenerate(string $channel, ?string $fixedDomain): callable
    {
        return static function () use ($channel, $fixedDomain): EmailInfo {
            $domain = $fixedDomain ?? self::pickDomain();
            $email = self::randomLocal() . '@' . $domain;
            // 服务免注册，Token 即邮箱地址本身
            return new EmailInfo($channel, $email, $email);
        };
    }

    /**
     * 生成 getEmails 闭包。
     *
     * @return callable(string,?string):Email[]
     */
    public static function makeGetEmails(): callable
    {
        return static function (string $email, ?string $token): array {
            $addr = trim($email);
            if ($addr === '') {
                throw new \InvalidArgumentException('mailmomy: 缺少邮箱地址');
            }

            $resp = HttpClient::get(
                self::BASE_URL . '/api/mail/messages',
                self::headers(),
                query: ['to' => $addr, 'page' => 1, 'limit' => 20],
            );
            $data = HttpClient::json($resp);
            $rows = is_array($data['emails'] ?? null) ? $data['emails'] : [];

            $out = [];
            foreach ($rows as $msg) {
                if (!is_array($msg)) {
                    continue;
                }
                $message = self::first($msg, 'message');
                $bodyText = self::first($msg, 'bodyText');
                $row = [
                    'id' => self::first($msg, 'id'),
                    'from' => self::first($msg, 'from'),
                    'to' => self::first($msg, 'recipient') ?: $addr,
                    'subject' => self::first($msg, 'subject'),
                    'text' => $bodyText !== '' ? $bodyText : $message,
                    'html' => $message,
                    'date' => self::first($msg, 'receivedAt'),
                ];
                $out[] = Normalize::email($row, $addr);
            }
            return $out;
        };
    }

    /**
     * 按优先级从消息字典中提取首个非空值并转为字符串。
     *
     * @param array<mixed> $msg
     */
    private static function first(array $msg, string ...$keys): string
    {
        foreach ($keys as $key) {
            $val = $msg[$key] ?? null;
            if ($val !== null && $val !== '') {
                if (is_scalar($val)) {
                    return (string) $val;
                }
            }
        }
        return '';
    }
}
