<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use ChanhanzhanX\TempMail\WebSocket;
use ChanhanzhanX\TempMail\ConfigStore;

/**
 * vip-215 渠道实现（vip.215.im）
 *
 * generate: GET 首页获取 cookie → POST /api/temp-inbox 创建收件箱
 * getEmails: WebSocket 连接 /v1/ws?token={ticket}，监听 message.new 事件
 */
final class Vip215
{
    private const CHANNEL = 'vip-215';
    private const BASE = 'https://vip.215.im';

    private const USER_AGENT = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36 Edg/148.0.0.0';

    private const SYNTHETIC_MARKER = '【tempmail-sdk|synthetic|vip-215|v1】';

    /** @var int 收信阻塞等待时间（毫秒） */
    private const GET_EMAILS_WAIT_MS = 5000;

    private const HEADERS = [
        'User-Agent'       => self::USER_AGENT,
        'Accept'           => '*/*',
        'Accept-Language'  => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
        'Cache-Control'    => 'no-cache',
        'Content-Type'     => 'application/json',
        'DNT'              => '1',
        'Origin'           => self::BASE,
        'Pragma'           => 'no-cache',
        'Referer'          => self::BASE . '/',
        'Sec-CH-UA'        => '"Chromium";v="148", "Microsoft Edge";v="148", "Not/A)Brand";v="99"',
        'Sec-CH-UA-Mobile' => '?0',
        'Sec-CH-UA-Platform' => '"Windows"',
        'Sec-Fetch-Dest'   => 'empty',
        'Sec-Fetch-Mode'   => 'cors',
        'Sec-Fetch-Site'   => 'same-origin',
        'X-Locale'         => 'zh',
    ];

    private const HOME_HEADERS = [
        'User-Agent'       => self::USER_AGENT,
        'Accept'           => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
        'Accept-Language'  => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
        'Cache-Control'    => 'no-cache',
        'DNT'              => '1',
        'Pragma'           => 'no-cache',
    ];

    /**
     * HTTP POST 创建临时收件箱。
     */
    public static function generate(): EmailInfo
    {
        // 获取首页 cookie
        $cookie = self::establishSession();

        $resp = HttpClient::post(
            self::BASE . '/api/temp-inbox',
            self::HEADERS + ['Cookie' => $cookie],
            body: '',
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('vip-215: create inbox failed: HTTP ' . $resp->getStatusCode());
        }

        $body = HttpClient::json($resp);
        if (empty($body['success'])) {
            throw new \RuntimeException('vip-215: success=false');
        }

        $data = $body['data'] ?? [];
        $address = $data['address'] ?? null;
        $token = $data['token'] ?? null;
        if (!$address || !$token) {
            throw new \RuntimeException('vip-215: missing address or token');
        }

        return new EmailInfo(
            self::CHANNEL,
            (string) $address,
            (string) $token,
            createdAt: $data['createdAt'] ?? null,
        );
    }

    /**
     * 通过 HTTP 接口获取邮件列表
     * GET https://vip.215.im/v1/messages
     * 返回完整邮件（含真实正文），相比 WebSocket 推送的合成卡片更完整
     *
     * @return array<int,array<string,mixed>>
     */
    private static function fetchMessagesViaHttp(string $jwt): array
    {
        $resp = HttpClient::get(
            self::BASE . '/v1/messages',
            self::HEADERS + ['Authorization' => 'Bearer ' . $jwt],
        );
        $status = $resp->getStatusCode();
        if ($status < 200 || $status >= 300) {
            throw new \RuntimeException('vip-215: /v1/messages HTTP ' . $status);
        }
        $body = HttpClient::json($resp);
        if (empty($body['success'])) {
            throw new \RuntimeException('vip-215: /v1/messages success=false');
        }
        $messages = $body['data']['messages'] ?? [];
        return is_array($messages) ? array_values(array_filter($messages, 'is_array')) : [];
    }

    /**
     * 通过 HTTP 详情接口获取单封邮件完整内容
     * GET https://vip.215.im/v1/messages/{id}
     *
     * @return array<string,mixed>|null
     */
    private static function fetchMessageDetail(string $jwt, string $id): ?array
    {
        $mid = trim($id);
        if ($mid === '') {
            return null;
        }
        try {
            $resp = HttpClient::get(
                self::BASE . '/v1/messages/' . rawurlencode($mid),
                self::HEADERS + ['Authorization' => 'Bearer ' . $jwt],
            );
            $status = $resp->getStatusCode();
            if ($status < 200 || $status >= 300) {
                return null;
            }
            $body = HttpClient::json($resp);
            if (empty($body['success'])) {
                return null;
            }
            $data = $body['data'] ?? null;
            return is_array($data) ? $data : null;
        } catch (\Throwable $e) {
            return null;
        }
    }

    /**
     * 判断行是否包含真实正文（用于决定是否补拉详情）
     */
    private static function hasRealBody(array $row): bool
    {
        foreach (['text', 'body_text', 'html', 'body_html', 'content', 'body'] as $key) {
            $v = $row[$key] ?? null;
            if (is_string($v) && trim($v) !== '') {
                return true;
            }
        }
        return false;
    }

    /**
     * 提取邮件 ID
     */
    private static function extractMessageId(array $row): string
    {
        foreach (['id', 'messageId', 'message_id'] as $key) {
            $v = $row[$key] ?? null;
            if (is_string($v) && trim($v) !== '') {
                return trim($v);
            }
            if (is_int($v) || is_float($v)) {
                return (string) (int) $v;
            }
        }
        return '';
    }

    /**
     * 获取邮件列表（HTTP 优先，WebSocket 回退）
     * 1. HTTP GET /v1/messages 拉取完整邮件（含真实正文）
     * 2. 缺正文的条目通过 GET /v1/messages/{id} 补拉详情
     * 3. HTTP 失败时回退到 WebSocket 阻塞收信（合成卡片）
     *
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('vip-215: token 不能为空');
        }

        /* 优先通过 HTTP 拉取完整邮件 */
        try {
            $messages = self::fetchMessagesViaHttp($token);
            $result = [];
            foreach ($messages as $row) {
                $id = self::extractMessageId($row);
                /* 若无真实正文，补拉详情 */
                if (!self::hasRealBody($row) && $id !== '') {
                    $detail = self::fetchMessageDetail($token, $id);
                    if ($detail !== null) {
                        $row = array_merge($row, $detail);
                    }
                }
                if (!isset($row['to'])) {
                    $row['to'] = $email;
                }
                $result[] = Normalize::email($row, $email);
            }
            return $result;
        } catch (\Throwable $httpErr) {
            /* HTTP 失败时回退到 WebSocket 阻塞收信 */
            return self::getEmailsViaWebSocket($email, $token);
        }
    }

    /**
     * WebSocket 阻塞式收信（回退路径）：连接后监听 message.new 事件。
     *
     * @return Email[]
     */
    private static function getEmailsViaWebSocket(string $email, string $token): array
    {
        // 获取 WebSocket ticket
        $ticket = self::fetchWsTicket($token);
        $url = 'wss://vip.215.im/v1/ws?token=' . rawurlencode($ticket);

        $ws = new WebSocket();
        $ws->connect($url, [
            'User-Agent' => self::USER_AGENT,
            'Origin'     => self::BASE,
        ]);

        try {
            $result = [];
            $deadline = microtime(true) + (self::GET_EMAILS_WAIT_MS / 1000.0);

            while (true) {
                $remaining = (int) (($deadline - microtime(true)) * 1000);
                if ($remaining <= 0) {
                    break;
                }
                $msg = $ws->receive($remaining);
                if ($msg === null) {
                    break;
                }

                $parsed = json_decode($msg, true);
                if (!is_array($parsed) || ($parsed['type'] ?? '') !== 'message.new') {
                    continue;
                }

                $data = $parsed['data'] ?? [];
                if (!is_array($data)) {
                    continue;
                }

                [$synText, $synHtml] = self::buildSyntheticBodies($email, $data);
                $flat = [
                    'id'      => $data['id'] ?? '',
                    'from'    => $data['from'] ?? '',
                    'subject' => $data['subject'] ?? '',
                    'date'    => $data['date'] ?? '',
                    'to'      => $email,
                    'text'    => $synText,
                    'html'    => $synHtml,
                ];
                $em = Normalize::email($flat, $email);
                if (!empty($em->id)) {
                    $result[] = $em;
                }
            }
            return $result;
        } finally {
            $ws->close();
        }
    }

    /**
     * 获取首页 Set-Cookie。
     */
    private static function establishSession(): string
    {
        $resp = HttpClient::get(self::BASE . '/', self::HOME_HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('vip-215: homepage failed');
        }

        $cookies = $resp->getHeader('Set-Cookie');
        $parts = [];
        foreach ($cookies as $raw) {
            $nv = explode(';', $raw, 2)[0];
            if (str_contains($nv, '=')) {
                $parts[] = trim($nv);
            }
        }
        $cookie = implode('; ', $parts);
        if (!str_contains($cookie, 'yyds_homepage_bridge=') || !str_contains($cookie, 'yyds_homepage_device=')) {
            throw new \RuntimeException('vip-215: missing homepage cookies');
        }
        return $cookie;
    }

    /**
     * 获取 WebSocket ticket。
     */
    private static function fetchWsTicket(string $jwt): string
    {
        $resp = HttpClient::get(
            self::BASE . '/v1/auth/ws-ticket',
            self::HEADERS + ['Authorization' => 'Bearer ' . $jwt],
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('vip-215: ws-ticket request failed');
        }
        $body = HttpClient::json($resp);
        if (empty($body['success'])) {
            throw new \RuntimeException('vip-215: ws-ticket success=false');
        }
        $ticket = ($body['data'] ?? [])['ticket'] ?? null;
        if (!$ticket) {
            throw new \RuntimeException('vip-215: missing ws ticket');
        }
        return (string) $ticket;
    }

    /**
     * 构建合成正文（无实际 body 时填充元数据摘要）。
     *
     * @return array{0:string,1:string} [text, html]
     */
    private static function buildSyntheticBodies(string $recipientEmail, array $data): array
    {
        $id   = self::sanitizeOneLine($data['id'] ?? '');
        $subj = self::sanitizeOneLine($data['subject'] ?? '');
        $from = self::sanitizeOneLine($data['from'] ?? '');
        $to   = self::sanitizeOneLine($recipientEmail);
        $date = self::sanitizeOneLine($data['date'] ?? '');

        $lines = [
            self::SYNTHETIC_MARKER,
            "id: {$id}",
            "subject: {$subj}",
            "from: {$from}",
            "to: {$to}",
            "date: {$date}",
        ];
        $size = $data['size'] ?? null;
        if (is_int($size) || is_float($size)) {
            if ($size >= 0) {
                $lines[] = 'size: ' . (int) $size;
            }
        }
        $text = implode("\n", $lines);

        // HTML 版本
        $pairs = [
            ['id', $id],
            ['subject', $subj],
            ['from', $from],
            ['to', $to],
            ['date', $date],
        ];
        $dlParts = '';
        foreach ($pairs as [$k, $v]) {
            $dlParts .= '<dt>' . htmlspecialchars($k) . '</dt><dd>' . htmlspecialchars($v) . '</dd>';
        }
        if ((is_int($size) || is_float($size)) && $size >= 0) {
            $sv = (string) (int) $size;
            $dlParts .= '<dt>size</dt><dd>' . htmlspecialchars($sv) . '</dd>';
        }
        $html = '<div class="tempmail-sdk-synthetic" data-tempmail-sdk-format="synthetic-v1" data-channel="vip-215">'
            . '<dl class="tempmail-sdk-meta">' . $dlParts . '</dl></div>';

        return [$text, $html];
    }

    private static function sanitizeOneLine(mixed $val): string
    {
        if ($val === null) {
            return '';
        }
        $s = (string) $val;
        $s = str_replace(["\r\n", "\r", "\n"], ' ', $s);
        return trim($s);
    }
}
