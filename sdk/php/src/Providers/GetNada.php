<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * GetNada 渠道实现（含全部别名域名）
 *
 * API: https://getnada.net/api
 * 流程: 获取公共域名列表 → 选定域名 → open 收件箱换取 token → 轮询消息列表并拉取详情。
 * 多个渠道共用同一套 API，仅固定域名与渠道标识不同，通过 makeGenerate/makeGetEmails 复用逻辑。
 */
final class GetNada
{
    private const API_BASE = 'https://getnada.net/api';

    /** @var array<string,string> */
    private const HEADERS_JSON = ['Accept' => 'application/json'];

    /** @var array<string,string> */
    private const HEADERS_POST = ['Accept' => 'application/json', 'Content-Type' => 'application/json'];

    /**
     * 生成随机本地部分（固定 sdk 前缀 + 16 位随机小写字母数字）。
     */
    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < 16; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return 'sdk' . $out;
    }

    /**
     * 校验并规范化单个域名，非法返回空串。
     */
    private static function cleanDomain(mixed $value): string
    {
        $domain = strtolower(trim((string) $value));
        if ($domain === '' || strlen($domain) > 253 || str_contains($domain, '..')) {
            return '';
        }
        $labels = explode('.', $domain);
        if (count($labels) < 2) {
            return '';
        }
        foreach ($labels as $label) {
            if (preg_match('/^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/i', $label) !== 1) {
                return '';
            }
        }
        return $domain;
    }

    /**
     * 拉取公共域名列表并选定目标域名。
     *
     * 指定 preferred 时必须命中；否则优先 getnada.net，退而取首个可用域名。
     */
    private static function pickDomain(?string $preferred): string
    {
        $resp = HttpClient::get(self::API_BASE . '/public/domains', self::HEADERS_JSON);
        $data = HttpClient::json($resp);
        $domains = is_array($data['domains'] ?? null) ? $data['domains'] : [];

        $cleaned = [];
        foreach ($domains as $domain) {
            $clean = self::cleanDomain($domain);
            if ($clean !== '') {
                $cleaned[] = $clean;
            }
        }

        $wanted = strtolower(ltrim(trim((string) ($preferred ?? '')), '@'));
        if ($wanted !== '') {
            foreach ($cleaned as $domain) {
                if ($domain === $wanted) {
                    return $domain;
                }
            }
            throw new \RuntimeException('getnada: 域名不可用: ' . $wanted);
        }
        foreach ($cleaned as $domain) {
            if ($domain === 'getnada.net') {
                return $domain;
            }
        }
        if (!empty($cleaned)) {
            return $cleaned[0];
        }
        throw new \RuntimeException('getnada: 无可用域名');
    }

    /**
     * 生成指定渠道的 generate 闭包。
     *
     * @param string|null $fixedDomain 固定域名（别名渠道）；为 null 时使用用户传入域名
     * @return callable(?string):EmailInfo
     */
    public static function makeGenerate(string $channel, ?string $fixedDomain): callable
    {
        return static function (?string $userDomain) use ($channel, $fixedDomain): EmailInfo {
            $preferred = $fixedDomain ?? $userDomain;
            $selected = self::pickDomain($preferred);
            $requested = self::randomLocal() . '@' . $selected;

            $resp = HttpClient::post(
                self::API_BASE . '/inbox/open',
                self::HEADERS_POST,
                json: ['email' => $requested],
            );
            $data = HttpClient::json($resp);

            $token = trim((string) ($data['token'] ?? ''));
            $email = trim((string) ($data['recipient'] ?? $requested));
            if ($token === '' || $email === '' || !str_contains($email, '@')) {
                throw new \RuntimeException($channel . ': open 响应无效');
            }
            $expiresAt = is_numeric($data['activeUntil'] ?? null) ? (int) $data['activeUntil'] : null;
            return new EmailInfo($channel, $email, $token, expiresAt: $expiresAt);
        };
    }

    /**
     * 拉取单封邮件详情，失败返回 null。
     *
     * @return array<mixed>|null
     */
    private static function fetchDetail(string $token, string $messageId): ?array
    {
        $url = self::API_BASE . '/inbox/message?id=' . rawurlencode($messageId) . '&token=' . rawurlencode($token);
        $resp = HttpClient::get($url, self::HEADERS_JSON);
        if ($resp->getStatusCode() >= 400) {
            return null;
        }
        $data = HttpClient::json($resp);
        if (is_array($data['message'] ?? null)) {
            return $data['message'];
        }
        return null;
    }

    /**
     * 将 GetNada 消息格式扁平化为 normalize 可识别的字段。
     *
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        $out = $raw;
        $out['id'] = $raw['id'] ?? '';
        $out['from'] = $raw['from_addr'] ?? $raw['from'] ?? '';
        $out['to'] = $raw['to'] ?? $recipient;
        $out['text'] = $raw['text_plain'] ?? $raw['text'] ?? '';
        $out['html'] = $raw['html_sanitized'] ?? $raw['html'] ?? '';
        if (array_key_exists('read_at', $raw)) {
            $out['read'] = (bool) $raw['read_at'];
        }
        return $out;
    }

    /**
     * 生成 getEmails 闭包。
     *
     * @return callable(string,?string):Email[]
     */
    public static function makeGetEmails(): callable
    {
        return static function (string $email, ?string $token): array {
            $auth = trim((string) $token);
            $address = trim($email);
            if ($auth === '') {
                throw new \InvalidArgumentException('getnada: token 不能为空');
            }
            if ($address === '') {
                throw new \InvalidArgumentException('getnada: 邮箱不能为空');
            }

            $resp = HttpClient::get(
                self::API_BASE . '/inbox/messages?token=' . rawurlencode($auth),
                self::HEADERS_JSON,
            );
            $data = HttpClient::json($resp);
            $rows = is_array($data['messages'] ?? null) ? $data['messages'] : [];

            $result = [];
            foreach ($rows as $item) {
                if (!is_array($item)) {
                    continue;
                }
                $messageId = trim((string) ($item['id'] ?? ''));
                $detail = $messageId !== '' ? self::fetchDetail($auth, $messageId) : null;
                $result[] = Normalize::email(self::flatten($detail ?? $item, $address), $address);
            }
            return $result;
        };
    }
}
