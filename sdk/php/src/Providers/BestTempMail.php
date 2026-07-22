<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * BestTempMail 渠道实现 — https://best-temp-mail.com
 *
 * 纯 JSON REST，无鉴权：客户端生成 UUID 作为 intToken；
 * POST /api/v3/createEmail 创建邮箱；POST /api/v3/getEmailList 拉取邮件。
 * token 序列化保存 intToken、id、update_tag。
 */
final class BestTempMail
{
    private const BASE = 'https://best-temp-mail.com';

    /** @var array<string,string> */
    private const HEADERS = [
        'Content-Type' => 'application/json',
        'Referer' => self::BASE . '/',
        'Origin' => self::BASE,
    ];

    /**
     * 生成 RFC 4122 版本 4 UUID。
     */
    private static function uuidv4(): string
    {
        $data = random_bytes(16);
        $data[6] = chr((ord($data[6]) & 0x0f) | 0x40);
        $data[8] = chr((ord($data[8]) & 0x3f) | 0x80);
        return vsprintf('%s%s-%s-%s-%s-%s%s%s', str_split(bin2hex($data), 4));
    }

    public static function generate(): EmailInfo
    {
        $intToken = self::uuidv4();
        $resp = HttpClient::post(
            self::BASE . '/api/v3/createEmail',
            self::HEADERS,
            json: ['intToken' => $intToken],
        );
        $body = HttpClient::json($resp);
        if (($body['status'] ?? null) !== 'success') {
            throw new \RuntimeException('best-temp-mail: 创建邮箱失败');
        }
        $data = $body['data'] ?? [];
        $address = (string) ($data['address'] ?? '');
        $emailId = $data['id'] ?? '';
        $updateTag = $data['update_tag'] ?? '';
        if ($address === '' || !str_contains($address, '@')) {
            throw new \RuntimeException('best-temp-mail: 返回的邮箱地址无效');
        }
        $token = json_encode([
            'intToken' => $intToken,
            'id' => $emailId,
            'update_tag' => $updateTag,
        ], JSON_UNESCAPED_SLASHES);
        return new EmailInfo('best-temp-mail', $address, (string) $token);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('best-temp-mail: token 不能为空');
        }
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('best-temp-mail: 邮箱地址不能为空');
        }
        $tok = json_decode($token, true);
        if (!is_array($tok)) {
            throw new \InvalidArgumentException('best-temp-mail: 无效的 token');
        }
        $intToken = (string) ($tok['intToken'] ?? '');
        $emailId = (string) ($tok['id'] ?? '');
        $updateTag = $tok['update_tag'] ?? '';
        if ($intToken === '' || $emailId === '') {
            throw new \InvalidArgumentException('best-temp-mail: token 中缺少必要字段 (intToken / id)');
        }

        $resp = HttpClient::post(
            self::BASE . '/api/v3/getEmailList',
            self::HEADERS,
            json: [
                'address' => $address,
                'id' => $emailId,
                'intToken' => $intToken,
                'update_tag' => $updateTag,
            ],
        );
        $body = HttpClient::json($resp);
        $data = $body['data'] ?? [];
        if (empty($data['hasNewEmail'])) {
            return [];
        }
        $rows = $data['emails'] ?? [];
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $raw = [
                'id' => (string) ($item['id'] ?? ''),
                'from' => $item['from'] ?? ($item['from_address'] ?? ($item['sender'] ?? '')),
                'to' => $address,
                'subject' => $item['subject'] ?? '',
                'text' => $item['text'] ?? ($item['body_text'] ?? ''),
                'html' => $item['html'] ?? ($item['body_html'] ?? ($item['body'] ?? '')),
                'date' => $item['date'] ?? ($item['created_at'] ?? ''),
            ];
            $out[] = Normalize::email($raw, $address);
        }
        return $out;
    }
}
