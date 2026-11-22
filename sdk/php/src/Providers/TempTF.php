<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * temp.tf 渠道实现
 *
 * https://temp.tf —— 创建返回一次性地址；收件列表响应按 bodyContentType 归位正文。
 */
final class TempTF
{
    private const API_BASE = 'https://temp.tf/api';

    /**
     * @return array<string,string>
     */
    private static function headers(): array
    {
        return ['Accept' => 'application/json', 'Content-Type' => 'application/json'];
    }

    /**
     * 获取一个一次性地址。
     */
    public static function generate(string $channel = 'temp-tf'): EmailInfo
    {
        $resp = HttpClient::get(self::API_BASE . '/account?plus=1', self::headers());
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('temp-tf: 创建邮箱失败');
        }
        $address = trim((string) (HttpClient::json($resp)['email'] ?? ''));
        if ($address === '') {
            throw new \RuntimeException('temp-tf: 创建响应缺少地址');
        }
        return new EmailInfo($channel, $address);
    }

    /**
     * 获取 temp.tf 邮件，body 按 bodyContentType 归位。
     *
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('temp-tf: 邮箱地址为空');
        }
        $resp = HttpClient::post(
            self::API_BASE . '/check',
            self::headers(),
            null,
            json_encode(['email' => $address], JSON_THROW_ON_ERROR),
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('temp-tf: 获取邮件失败');
        }

        $out = [];
        foreach (HttpClient::json($resp)['data'] ?? [] as $item) {
            if (!is_array($item)) {
                continue;
            }
            $isText = strtolower((string) ($item['bodyContentType'] ?? '')) === 'text';
            $body = (string) ($item['body'] ?? '');
            $out[] = Normalize::email([
                'id' => $item['id'] ?? '',
                'from' => $item['from'] ?? '',
                'to' => $address,
                'subject' => $item['subject'] ?? '',
                'text' => $isText ? $body : '',
                'html' => $isText ? '' : $body,
                'date' => $item['date'] ?? '',
                'attachments' => $item['attachments'] ?? [],
            ], $address);
        }
        return $out;
    }
}
