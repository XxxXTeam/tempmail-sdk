<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * smailpro.com 渠道实现
 *
 * 两段式流程：
 *   1. GET /app/payload?url={目标API}[&email=&mid=] → 返回 JWT（纯文本）
 *   2. 携带 JWT 调用 api.sonjj.com 对应接口（payload={JWT}）
 * 无需持久化认证信息，getEmails 仅依赖邮箱地址；token 仅占位。
 */
final class Smailpro
{
    private const CHANNEL = 'smailpro';
    private const BASE_URL = 'https://smailpro.com';
    private const API_BASE_URL = 'https://api.sonjj.com/v1/temp_email';

    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
        'Accept' => 'application/json, text/plain, */*',
        'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
        'Referer' => 'https://smailpro.com/',
    ];

    /**
     * 获取访问 sonjj API 所需的 JWT。
     *
     * @param array<string,string> $extra 附加查询参数（email、mid）
     */
    private static function fetchPayload(string $targetUrl, array $extra = []): string
    {
        $query = ['url' => $targetUrl] + $extra;
        $resp = HttpClient::get(self::BASE_URL . '/app/payload', self::HEADERS, query: $query);
        $payload = trim((string) $resp->getBody(), " \t\n\r\0\x0B\"");
        if ($payload === '') {
            throw new \RuntimeException('smailpro: payload 为空');
        }
        return $payload;
    }

    /**
     * 携带 JWT 调用 sonjj API 并返回解析后的 JSON。
     *
     * @param array<string,string> $extra
     * @return array<mixed>
     */
    private static function callApi(string $targetUrl, array $extra = []): array
    {
        $payload = self::fetchPayload($targetUrl, $extra);
        $resp = HttpClient::get($targetUrl, self::HEADERS, query: ['payload' => $payload]);
        return HttpClient::json($resp);
    }

    public static function generate(string $channel = self::CHANNEL): EmailInfo
    {
        $data = self::callApi(self::API_BASE_URL . '/create');
        $email = trim((string) ($data['email'] ?? ''));
        if ($email === '') {
            throw new \RuntimeException('smailpro: 创建邮箱失败, 未返回邮箱地址');
        }
        $expiresAt = $data['expired_at'] ?? null;
        return new EmailInfo($channel, $email, $email, expiresAt: is_numeric($expiresAt) ? (int) $expiresAt : null);
    }

    /**
     * 获取单封邮件正文，返回 [html, text]；失败返回空串。
     *
     * @return array{0:string,1:string}
     */
    private static function fetchMessage(string $email, string $mid): array
    {
        if ($mid === '') {
            return ['', ''];
        }
        try {
            $data = self::callApi(self::API_BASE_URL . '/message', ['email' => $email, 'mid' => $mid]);
        } catch (\Throwable) {
            return ['', ''];
        }
        return [(string) ($data['body'] ?? ''), (string) ($data['textBody'] ?? '')];
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('smailpro: 邮箱地址为空');
        }

        $data = self::callApi(self::API_BASE_URL . '/inbox', ['email' => $addr]);
        $inner = $data['data'] ?? null;
        $messages = is_array($inner) ? ($inner['messages'] ?? null) : ($data['messages'] ?? null);
        if (!is_array($messages) || empty($messages)) {
            return [];
        }

        $out = [];
        foreach ($messages as $m) {
            if (!is_array($m)) {
                continue;
            }
            $mid = trim((string) ($m['mid'] ?? ''));
            [$htmlBody, $textBody] = self::fetchMessage($addr, $mid);
            $raw = [
                'id' => $mid,
                'from' => $m['from'] ?? '',
                'to' => $addr,
                'subject' => $m['subject'] ?? '',
                'date' => $m['datetime'] ?? '',
            ];
            if ($htmlBody !== '' || $textBody !== '') {
                $raw['html'] = $htmlBody;
                $raw['text'] = $textBody;
            }
            $out[] = Normalize::email($raw, $addr);
        }
        return $out;
    }
}
