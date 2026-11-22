<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * GeniusGSM 渠道实现
 *
 * https://tempmail.geniusgsm.com —— 创建收件箱返回内部 ID；
 * 收件箱列表响应已包含 body_text / body_html。
 */
final class GeniusGSM
{
    private const API_BASE = 'https://tempmail.geniusgsm.com/api';

    /**
     * @return array<string,string>
     */
    private static function headers(): array
    {
        return ['Accept' => 'application/json', 'Content-Type' => 'application/json'];
    }

    /**
     * @return array<int|string,mixed>
     */
    private static function rows(mixed $data): array
    {
        if (is_array($data)) {
            if (array_is_list($data)) {
                return $data;
            }
            if (isset($data['results']) && is_array($data['results'])) {
                return $data['results'];
            }
        }
        return [];
    }

    private static function fromValue(array $raw): string
    {
        $name = (string) ($raw['from_name'] ?? '');
        $addr = (string) ($raw['from_email'] ?? '');
        return $name === '' || $addr === '' ? $addr : $name . ' <' . $addr . '>';
    }

    /**
     * 选择激活域名并创建收件箱。
     */
    public static function generate(string $channel = 'geniusgsm'): EmailInfo
    {
        $domainResp = HttpClient::get(self::API_BASE . '/domains/', self::headers());
        if ($domainResp->getStatusCode() >= 400) {
            throw new \RuntimeException('geniusgsm: 获取域名失败');
        }
        $domainId = 0;
        foreach (self::rows(HttpClient::json($domainResp)) as $item) {
            if (is_array($item) && ($item['is_active'] ?? false)) {
                $domainId = (int) ($item['id'] ?? 0);
                break;
            }
        }
        if ($domainId === 0) {
            throw new \RuntimeException('geniusgsm: 无可用激活域名');
        }

        $resp = HttpClient::post(
            self::API_BASE . '/inboxes/create_inbox/',
            self::headers(),
            null,
            json_encode(['domain_id' => $domainId, 'ttl' => 3600], JSON_THROW_ON_ERROR),
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('geniusgsm: 创建邮箱失败');
        }
        $data = HttpClient::json($resp);
        $id = trim((string) ($data['id'] ?? ''));
        $address = trim((string) ($data['email'] ?? ''));
        if ($id === '' || $address === '') {
            throw new \RuntimeException('geniusgsm: 创建响应不完整');
        }
        return new EmailInfo($channel, $address, $id, null, $data['created_at'] ?? null);
    }

    /**
     * 获取 GeniusGSM 邮件，列表响应已包含完整正文。
     *
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $inboxId = trim((string) $token);
        if ($inboxId === '') {
            throw new \InvalidArgumentException('geniusgsm: 收件箱 ID 为空');
        }
        $resp = HttpClient::get(
            self::API_BASE . '/inboxes/' . rawurlencode($inboxId) . '/emails/',
            self::headers(),
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('geniusgsm: 获取邮件失败');
        }

        $out = [];
        foreach (self::rows(HttpClient::json($resp)) as $item) {
            if (!is_array($item)) {
                continue;
            }
            $out[] = Normalize::email([
                'id' => $item['id'] ?? '',
                'from' => self::fromValue($item),
                'to' => $item['inbox_email'] ?? $email,
                'subject' => $item['subject'] ?? '',
                'text' => $item['body_text'] ?? '',
                'html' => $item['body_html'] ?? '',
                'date' => $item['received_at'] ?? '',
                'read' => $item['is_read'] ?? false,
                'attachments' => $item['attachments'] ?? [],
            ], $email);
        }
        return $out;
    }
}
