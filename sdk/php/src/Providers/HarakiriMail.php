<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * HarakiriMail 渠道实现 — https://harakirimail.com
 *
 * 无认证、无 Cookie、无 Token 的 REST API：本地生成随机收件箱名拼固定域名，
 * 列表 GET /api/v1/inbox/{name}，逐封 GET /api/v1/email/{id} 补全正文。
 */
final class HarakiriMail
{
    private const CHANNEL = 'harakirimail';
    private const BASE = 'https://harakirimail.com';
    private const DOMAIN = 'harakirimail.com';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json, text/plain, */*',
        'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    private static function randomName(int $length = 12): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(): EmailInfo
    {
        $name = self::randomName();
        $email = $name . '@' . self::DOMAIN;

        // 可选：调用收件箱接口验证地址可用
        $resp = HttpClient::get(self::BASE . '/api/v1/inbox/' . rawurlencode($name), self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('harakirimail: 收件箱请求失败 ' . $resp->getStatusCode());
        }

        return new EmailInfo(self::CHANNEL, $email);
    }

    /**
     * 获取单封邮件正文，失败返回空数组。
     *
     * @return array<mixed>
     */
    private static function fetchBody(string $emailId): array
    {
        if ($emailId === '') {
            return [];
        }
        try {
            $resp = HttpClient::get(self::BASE . '/api/v1/email/' . rawurlencode($emailId), self::HEADERS);
            if ($resp->getStatusCode() < 400) {
                $data = HttpClient::json($resp);
                if (!empty($data)) {
                    return $data;
                }
            }
        } catch (\Throwable) {
            // 忽略详情获取失败
        }
        return [];
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $e = trim($email);
        if ($e === '') {
            throw new \InvalidArgumentException('harakirimail: 邮箱地址为空');
        }
        $name = str_contains($e, '@') ? substr($e, 0, strrpos($e, '@')) : $e;

        $resp = HttpClient::get(self::BASE . '/api/v1/inbox/' . rawurlencode($name), self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('harakirimail: 收件箱请求失败 ' . $resp->getStatusCode());
        }
        $data = HttpClient::json($resp);
        $rows = $data['emails'] ?? null;
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $raw) {
            if (!is_array($raw)) {
                continue;
            }
            $emailId = (string) ($raw['_id'] ?? '');
            $detail = self::fetchBody($emailId);
            $out[] = Normalize::email([
                'id' => $emailId,
                'from' => $raw['from'] ?? '',
                'to' => $e,
                'subject' => $raw['subject'] ?? '',
                'date' => $raw['received'] ?? '',
                'html' => $detail['body_html'] ?? ($detail['html'] ?? ''),
                'text' => $detail['body_text'] ?? ($detail['text'] ?? ''),
                'isRead' => false,
            ], $e);
        }
        return $out;
    }
}
