<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Tsukaisute Mail 渠道实现
 *
 * https://tsukaisutemail.com —— 无需密钥；创建随机地址后，地址即读取凭据。
 */
final class TsukaisuteMail
{
    private const API_BASE = 'https://tsukaisutemail.com/api/v1';

    /** @var array<string,string> */
    private const HEADERS = ['Accept' => 'application/json'];

    /**
     * @return array<string,mixed>
     */
    private static function getJson(string $url): array
    {
        $resp = HttpClient::get($url, self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('tsukaisutemail: 请求失败');
        }
        return HttpClient::json($resp);
    }

    /**
     * 创建 Tsukaisute Mail 临时地址。
     */
    public static function generate(string $channel = 'tsukaisutemail'): EmailInfo
    {
        $resp = HttpClient::post(self::API_BASE . '/address', self::HEADERS);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('tsukaisutemail: 创建地址失败');
        }
        $address = trim((string) (HttpClient::json($resp)['address'] ?? ''));
        if ($address === '') {
            throw new \RuntimeException('tsukaisutemail: 创建响应缺少地址');
        }
        return new EmailInfo($channel, $address);
    }

    /**
     * 获取 Tsukaisute Mail 邮件，逐条补全正文。
     *
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('tsukaisutemail: 邮箱地址为空');
        }
        $path = str_replace('%40', '@', rawurlencode($address));
        $rows = self::getJson(self::API_BASE . '/inbox/' . $path)['messages'] ?? null;
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
                $detail = self::getJson(self::API_BASE . '/message/' . rawurlencode($messageId));
                $message = $detail['message'] ?? null;
                if (is_array($message)) {
                    $out[] = Normalize::email([
                        'id' => $message['id'] ?? '',
                        'from' => $message['from_address'] ?? '',
                        'to' => $address,
                        'subject' => $message['subject'] ?? '',
                        'text' => $message['text_body'] ?? '',
                        'html' => $message['html_body'] ?? '',
                        'date' => $message['received_at'] ?? '',
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
                'text' => $item['snippet'] ?? '',
                'date' => $item['received_at'] ?? '',
            ], $address);
        }
        return $out;
    }
}
