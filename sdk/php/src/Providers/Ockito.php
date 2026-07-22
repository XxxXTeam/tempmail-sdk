<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * ockito 渠道实现 — https://ockito.com/web-api
 *
 * POST /gtoken 取 access/refresh token；GET /email 取邮箱；
 * GET /retrieve/{email}/emails 列表，GET /retrieve/{email}/{uid} 详情，
 * Bearer 鉴权，401 时用 refresh token 走 /grefresh 刷新。
 * Token 打包为 JSON {access_token, refresh_token} 存入 EmailInfo。
 */
final class Ockito
{
    private const CHANNEL = 'ockito';
    private const BASE = 'https://ockito.com/web-api';

    /** @var array<string,string> */
    private const DEFAULT_HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36',
    ];

    /**
     * 请求 JSON，返回 [状态码, 解析结果]；解析失败或非 2xx 抛出异常（消息含状态码）。
     *
     * @param array<string,string> $headers
     * @param array<mixed>|null    $payload
     * @return array{0:int,1:array<mixed>}
     */
    private static function fetchJson(string $path, string $method = 'GET', array $headers = [], ?array $payload = null): array
    {
        $requestHeaders = self::DEFAULT_HEADERS + $headers;
        if ($method === 'POST') {
            $resp = HttpClient::post(self::BASE . $path, $requestHeaders, json: $payload ?? []);
        } else {
            $resp = HttpClient::get(self::BASE . $path, $requestHeaders);
        }
        $status = $resp->getStatusCode();
        $text = (string) $resp->getBody();
        $data = $text !== '' ? json_decode($text, true) : [];
        if ($text !== '' && !is_array($data)) {
            throw new \RuntimeException('ockito invalid JSON: ' . $path . ' HTTP ' . $status);
        }
        if ($status < 200 || $status >= 300) {
            throw new \RuntimeException('ockito http ' . $status);
        }
        return [$status, is_array($data) ? $data : []];
    }

    private static function packToken(string $accessToken, string $refreshToken): string
    {
        return (string) json_encode(['access_token' => $accessToken, 'refresh_token' => $refreshToken]);
    }

    /**
     * @return array{0:string,1:string}
     */
    private static function unpackToken(?string $token): array
    {
        $value = trim((string) $token);
        if ($value === '' || !str_starts_with($value, '{')) {
            throw new \InvalidArgumentException('ockito: 无效的会话 token');
        }
        $data = json_decode($value, true);
        if (!is_array($data)) {
            throw new \InvalidArgumentException('ockito: 无效的会话 token');
        }
        $accessToken = trim((string) ($data['access_token'] ?? ''));
        $refreshToken = trim((string) ($data['refresh_token'] ?? ''));
        if ($accessToken === '' || $refreshToken === '') {
            throw new \InvalidArgumentException('ockito: 无效的会话 token');
        }
        return [$accessToken, $refreshToken];
    }

    private static function refreshAccessToken(string $refreshToken): string
    {
        [, $data] = self::fetchJson('/grefresh', 'POST', [
            'Authorization' => 'Bearer ' . $refreshToken,
            'X-PASSTHROUGH' => 'Y',
        ]);
        $accessToken = trim((string) ($data['access_token'] ?? ''));
        if ($accessToken === '') {
            throw new \RuntimeException('ockito: 无效的刷新响应');
        }
        return $accessToken;
    }

    /**
     * Bearer 请求；遇 401 用 refresh token 刷新后重试。
     *
     * @return array<mixed>
     */
    private static function fetchBearerJson(string $path, string $accessToken, string $refreshToken): array
    {
        try {
            [, $data] = self::fetchJson($path, 'GET', ['Authorization' => 'Bearer ' . $accessToken]);
            return $data;
        } catch (\RuntimeException $exc) {
            if (!str_contains($exc->getMessage(), 'http 401')) {
                throw $exc;
            }
        }
        $refreshed = self::refreshAccessToken($refreshToken);
        [, $data] = self::fetchJson($path, 'GET', ['Authorization' => 'Bearer ' . $refreshed]);
        return $data;
    }

    /**
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flattenInboxRow(array $raw, string $recipient): array
    {
        return [
            'id' => $raw['uid'] ?? '',
            'from' => $raw['sender'] ?? '',
            'to' => $recipient,
            'subject' => $raw['subject'] ?? '',
            'text' => $raw['snippet'] ?? '',
            'html' => $raw['html'] ?? '',
            'date' => $raw['timestamp'] ?? '',
            'is_read' => false,
            'attachments' => [],
        ];
    }

    /**
     * @param array<mixed> $raw
     * @return array<mixed>
     */
    private static function flattenMessage(array $raw, string $recipient): array
    {
        $obj = is_array($raw['obj'] ?? null) ? $raw['obj'] : $raw;
        return [
            'id' => $raw['uid'] ?? '',
            'from' => $obj['sender_email'] ?? $obj['SenderEmail'] ?? $obj['from_'] ?? $obj['From']
                ?? $obj['from'] ?? $obj['sender_name'] ?? $obj['SenderName'] ?? '',
            'to' => $obj['to'] ?? $obj['To'] ?? $recipient,
            'subject' => $obj['subject'] ?? $obj['Subject'] ?? '',
            'text' => $obj['text'] ?? '',
            'html' => $obj['html'] ?? '',
            'date' => $obj['date'] ?? $obj['Date'] ?? '',
            'is_read' => false,
            'attachments' => [],
        ];
    }

    public static function generate(): EmailInfo
    {
        [, $login] = self::fetchJson('/gtoken', 'POST', [], []);
        $accessToken = trim((string) ($login['access_token'] ?? ''));
        $refreshToken = trim((string) ($login['refresh_token'] ?? ''));
        if ($accessToken === '' || $refreshToken === '') {
            throw new \RuntimeException('ockito: 无效的 token 响应');
        }

        [, $emailData] = self::fetchJson('/email', 'GET', ['Authorization' => 'Bearer ' . $accessToken]);
        $emailValue = $emailData['email'] ?? null;
        $email = '';
        if (is_string($emailValue)) {
            $email = trim($emailValue);
        } elseif (is_array($emailValue)) {
            $email = trim((string) ($emailValue['email'] ?? ''));
        }
        if ($email === '' || !str_contains($email, '@')) {
            throw new \RuntimeException('ockito: 无效的邮箱响应');
        }

        return new EmailInfo(self::CHANNEL, $email, self::packToken($accessToken, $refreshToken));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        [$accessToken, $refreshToken] = self::unpackToken($token);
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('ockito: 邮箱地址为空');
        }

        $data = self::fetchBearerJson('/retrieve/' . rawurlencode($addr) . '/emails', $accessToken, $refreshToken);
        $rows = $data['inbox'] ?? [];
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                continue;
            }
            $uid = trim((string) ($row['uid'] ?? ''));
            if ($uid === '') {
                $out[] = Normalize::email(self::flattenInboxRow($row, $addr), $addr);
                continue;
            }
            try {
                $detail = self::fetchBearerJson(
                    '/retrieve/' . rawurlencode($addr) . '/' . rawurlencode($uid),
                    $accessToken,
                    $refreshToken,
                );
                $out[] = Normalize::email(self::flattenMessage($detail, $addr), $addr);
            } catch (\Throwable) {
                $out[] = Normalize::email(self::flattenInboxRow($row, $addr), $addr);
            }
        }
        return $out;
    }
}
