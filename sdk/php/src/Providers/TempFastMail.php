<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * tempfastmail.com 渠道实现
 *
 * API: https://tempfastmail.com
 * 流程: POST /api/email-box 创建邮箱（返回 email + uuid，uuid 作为 token）→
 *       GET /api/email-box/{uuid}/emails 列表 → GET /api/email-box/{uuid}/email/{id} 详情。
 */
final class TempFastMail
{
    private const CHANNEL = 'tempfastmail';
    private const BASE_URL = 'https://tempfastmail.com';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'User-Agent' => 'Mozilla/5.0',
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::BASE_URL . '/api/email-box', self::HEADERS);
        $data = HttpClient::json($resp);
        $email = trim((string) ($data['email'] ?? ''));
        $uuid = trim((string) ($data['uuid'] ?? ''));
        if ($email === '' || !str_contains($email, '@') || $uuid === '') {
            throw new \RuntimeException('tempfastmail: 创建邮箱响应无效');
        }
        return new EmailInfo(self::CHANNEL, $email, $uuid);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $uuid = trim((string) $token);
        $address = trim($email);
        if ($uuid === '') {
            throw new \InvalidArgumentException('tempfastmail: token 不能为空');
        }
        if ($address === '') {
            throw new \InvalidArgumentException('tempfastmail: 邮箱地址不能为空');
        }

        $resp = HttpClient::get(self::BASE_URL . '/api/email-box/' . rawurlencode($uuid) . '/emails', self::HEADERS);
        $rows = HttpClient::json($resp);
        if (!array_is_list($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $row) {
            $raw = is_array($row) ? $row : [];
            $messageId = trim((string) ($raw['uuid'] ?? ''));
            if ($messageId !== '') {
                $detailResp = HttpClient::get(
                    self::BASE_URL . '/api/email-box/' . rawurlencode($uuid) . '/email/' . rawurlencode($messageId),
                    self::HEADERS,
                );
                if ($detailResp->getStatusCode() < 400) {
                    $detail = HttpClient::json($detailResp);
                    if (!empty($detail)) {
                        $raw = $detail;
                    }
                }
            }
            $out[] = Normalize::email(self::flatten($raw, $address), $address);
        }
        return $out;
    }

    /**
     * @param array<mixed> $raw
     * @return array<string,mixed>
     */
    private static function flatten(array $raw, string $recipient): array
    {
        return [
            'id' => $raw['uuid'] ?? '',
            'from' => $raw['from'] ?? '',
            'to' => $raw['real_to'] ?? $recipient,
            'subject' => $raw['subject'] ?? '',
            'html' => $raw['html'] ?? '',
            'date' => $raw['received_at'] ?? '',
            'attachments' => $raw['attachments'] ?? [],
        ];
    }
}
