<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * MailboxTemp 渠道实现
 *
 * https://mailboxtemp.com —— 无需密钥；创建免费邮箱后，地址即读取凭据。
 */
final class MailboxTemp
{
    private const API_BASE = 'https://mailboxtemp.com';

    /** @var array<string,string> */
    private const HEADERS = ['Accept' => 'application/json'];

    /**
     * @return array<string,mixed>
     */
    private static function getJson(string $url): array
    {
        $resp = HttpClient::get($url, self::HEADERS);
        $data = HttpClient::json($resp);
        if ($resp->getStatusCode() >= 400 || ($data['ok'] ?? true) === false) {
            throw new \RuntimeException('mailboxtemp: 请求失败');
        }
        return $data;
    }

    /**
     * 创建 MailboxTemp 临时邮箱。
     */
    public static function generate(string $channel = 'mailboxtemp'): EmailInfo
    {
        $resp = HttpClient::post(self::API_BASE . '/api/inbox/generate', self::HEADERS);
        $data = HttpClient::json($resp);
        $address = trim((string) ($data['address'] ?? ''));
        if ($resp->getStatusCode() >= 400 || ($data['ok'] ?? true) === false || $address === '') {
            throw new \RuntimeException('mailboxtemp: 创建邮箱失败');
        }
        return new EmailInfo($channel, $address);
    }

    /**
     * 获取 MailboxTemp 邮件，逐条补全正文。
     *
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('mailboxtemp: 邮箱地址为空');
        }
        $path = rawurlencode($address);
        $rows = self::getJson(self::API_BASE . '/api/inbox/' . $path . '/emails')['emails'] ?? null;
        if (!is_array($rows)) {
            return [];
        }

        $out = [];
        foreach ($rows as $item) {
            if (!is_array($item)) {
                continue;
            }
            $messageId = trim((string) ($item['id'] ?? ''));
            if ($messageId === '') {
                continue;
            }
            try {
                $detail = self::getJson(
                    self::API_BASE . '/api/email/' . $path . '/' . rawurlencode($messageId),
                );
                $message = $detail['email'] ?? null;
                if (is_array($message)) {
                    $out[] = Normalize::email([
                        'id' => $message['id'] ?? '',
                        'from' => $message['from_address'] ?? '',
                        'to' => $address,
                        'subject' => $message['subject'] ?? '',
                        'text' => $message['body_text'] ?? '',
                        'html' => $message['body_html'] ?? '',
                        'date' => $message['received_at'] ?? '',
                        'read' => $message['read'] ?? false,
                        'attachments' => $detail['attachments'] ?? [],
                    ], $address);
                    continue;
                }
            } catch (\RuntimeException) {
                // 详情失败时退回摘要
            }
            $out[] = Normalize::email([
                'id' => $messageId,
                'from' => $item['from_address'] ?? '',
                'to' => $address,
                'subject' => $item['subject'] ?? '',
                'date' => $item['received_at'] ?? '',
                'read' => $item['read'] ?? false,
            ], $address);
        }
        return $out;
    }
}
