<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * DummyInbox 渠道实现
 *
 * https://dummyinbox.com —— 无需密钥；创建随机地址后，地址本身即读取凭据。
 */
final class DummyInbox
{
    private const API_BASE = 'https://dummyinbox.com/api/v1';

    /** @var array<string,string> */
    private const HEADERS = ['Accept' => 'application/json'];

    private static function pathAddress(string $email): string
    {
        return str_replace('%40', '@', rawurlencode(trim($email)));
    }

    /**
     * 创建 DummyInbox 临时邮箱。
     */
    public static function generate(string $channel = 'dummyinbox'): EmailInfo
    {
        $resp = HttpClient::post(self::API_BASE . '/inboxes', self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('dummyinbox: 创建邮箱失败');
        }
        $data = HttpClient::json($resp);
        $address = trim((string) ($data['address'] ?? ''));
        if ($address === '') {
            throw new \RuntimeException('dummyinbox: 创建响应缺少地址');
        }
        return new EmailInfo($channel, $address);
    }

    /**
     * 获取 DummyInbox 邮件，逐条补全正文。
     *
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('dummyinbox: 邮箱地址为空');
        }
        $path = self::pathAddress($address);
        $resp = HttpClient::get(self::API_BASE . '/inboxes/' . $path . '/messages?limit=50', self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('dummyinbox: 拉取邮件失败');
        }
        $rows = HttpClient::json($resp)['messages'] ?? null;
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
            $detailResp = HttpClient::get(
                self::API_BASE . '/inboxes/' . $path . '/messages/' . rawurlencode($messageId),
                self::HEADERS,
            );
            if ($detailResp->getStatusCode() < 400) {
                $message = HttpClient::json($detailResp)['message'] ?? null;
                if (is_array($message)) {
                    $to = $message['to'] ?? $address;
                    if (is_array($to)) {
                        $to = $to[array_key_first($to)] ?? $address;
                    }
                    $out[] = Normalize::email([
                        'id' => $message['id'] ?? '',
                        'from' => $message['from'] ?? '',
                        'to' => $to ?: $address,
                        'subject' => $message['subject'] ?? '',
                        'text' => $message['text'] ?? '',
                        'html' => $message['html'] ?? '',
                        'date' => $message['receivedAt'] ?? '',
                        'attachments' => $message['attachments'] ?? [],
                    ], $address);
                    continue;
                }
            }
            // 详情失败时退回摘要
            $out[] = Normalize::email([
                'id' => $messageId,
                'from' => $item['from'] ?? '',
                'to' => $address,
                'subject' => $item['subject'] ?? '',
                'text' => $item['snippet'] ?? '',
                'date' => $item['receivedAt'] ?? '',
            ], $address);
        }
        return $out;
    }
}
