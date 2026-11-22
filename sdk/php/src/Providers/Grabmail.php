<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * GrabMail 渠道实现
 *
 * https://grabmail.io —— 地址即邮箱：首封邮件到达时自动存在，无需创建。
 */
final class Grabmail
{
    private const API_BASE = 'https://grabmail.io/api/v1';
    private const DOMAIN = 'grabmail.io';

    /** @var array<string,string> */
    private const HEADERS = ['Accept' => 'application/json'];

    private static function fromValue(array $raw): string
    {
        $name = (string) ($raw['from_name'] ?? '');
        $addr = (string) ($raw['from'] ?? '');
        return $name === '' ? $addr : $name . ' <' . $addr . '>';
    }

    /**
     * @return array<string,mixed>
     */
    private static function getJson(string $url): array
    {
        $resp = HttpClient::get($url, self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('grabmail: 请求失败');
        }
        return HttpClient::json($resp);
    }

    /**
     * 生成 GrabMail 地址（无需请求服务端）。
     */
    public static function generate(string $channel = 'grabmail'): EmailInfo
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $local = 'sdk';
        for ($i = 0; $i < 16; $i++) {
            $local .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return new EmailInfo($channel, $local . '@' . self::DOMAIN);
    }

    /**
     * 获取 GrabMail 邮件，逐条补全正文。
     *
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('grabmail: 邮箱地址为空');
        }
        $rows = self::getJson(
            self::API_BASE . '/mailbox?address=' . str_replace('%40', '@', rawurlencode($address)),
        )['messages'] ?? null;
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
                    self::API_BASE . '/message/' . rawurlencode($messageId)
                    . '?mailbox=' . str_replace('%40', '@', rawurlencode($address)),
                );
                $out[] = Normalize::email([
                    'id' => $detail['id'] ?? $messageId,
                    'from' => self::fromValue($detail),
                    'to' => $address,
                    'subject' => $detail['subject'] ?? '',
                    'text' => $detail['text'] ?? '',
                    'html' => $detail['html'] ?? '',
                    'date' => $detail['date'] ?? '',
                    'seen' => $detail['seen'] ?? false,
                    'attachments' => $detail['attachments'] ?? [],
                ], $address);
                continue;
            } catch (\RuntimeException) {
                // 详情失败时退回摘要
            }
            $out[] = Normalize::email([
                'id' => $messageId,
                'from' => self::fromValue($item),
                'to' => $address,
                'subject' => $item['subject'] ?? '',
                'text' => $item['preview'] ?? '',
                'date' => $item['date'] ?? '',
                'seen' => $item['seen'] ?? false,
            ], $address);
        }
        return $out;
    }
}
