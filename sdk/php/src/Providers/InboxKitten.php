<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * InboxKitten 渠道实现 — https://inboxkitten.com
 *
 * 本地生成随机邮箱，列表 GET /api/v1/mail/list?recipient={local}，
 * 逐封通过 storage 的 region/key 调用 getKey（元数据）+ getHtml（正文）补全。
 */
final class InboxKitten
{
    private const CHANNEL = 'inboxkitten';
    private const API_BASE = 'https://inboxkitten.com/api/v1/mail';
    private const DOMAIN = 'inboxkitten.com';

    /** @var array<string,string> */
    private const HEADERS_JSON = ['Accept' => 'application/json'];

    /** @var array<string,string> */
    private const HEADERS_HTML = ['Accept' => 'text/html,*/*'];

    private static function randomLocal(): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = 'sdk';
        for ($i = 0; $i < 16; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    public static function generate(): EmailInfo
    {
        return new EmailInfo(self::CHANNEL, self::randomLocal() . '@' . self::DOMAIN);
    }

    /**
     * 获取单封元数据。
     *
     * @return array<mixed>
     */
    private static function fetchMeta(string $region, string $key): array
    {
        $resp = HttpClient::get(
            self::API_BASE . '/getKey?region=' . rawurlencode($region) . '&key=' . rawurlencode($key),
            self::HEADERS_JSON,
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('inboxkitten: getKey 失败 ' . $resp->getStatusCode());
        }
        return HttpClient::json($resp);
    }

    private static function fetchHtml(string $region, string $key): string
    {
        $resp = HttpClient::get(
            self::API_BASE . '/getHtml?region=' . rawurlencode($region) . '&key=' . rawurlencode($key),
            self::HEADERS_HTML,
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('inboxkitten: getHtml 失败 ' . $resp->getStatusCode());
        }
        return (string) $resp->getBody();
    }

    /**
     * 将列表条目转换为标准化前的原始字段，尽力补全正文。
     *
     * @param array<mixed> $row
     * @return array<mixed>
     */
    private static function detailRaw(array $row, string $recipient): array
    {
        $storage = is_array($row['storage'] ?? null) ? $row['storage'] : [];
        $message = is_array($row['message'] ?? null) ? $row['message'] : [];
        $headers = is_array($message['headers'] ?? null) ? $message['headers'] : [];
        $envelope = is_array($row['envelope'] ?? null) ? $row['envelope'] : [];
        $region = trim((string) ($storage['region'] ?? ''));
        $key = trim((string) ($storage['key'] ?? ''));

        $raw = [
            'id' => $key,
            'messageId' => $key,
            'from' => $headers['from'] ?? ($envelope['sender'] ?? ''),
            'to' => $row['recipient'] ?? ($envelope['targets'] ?? $recipient),
            'subject' => $headers['subject'] ?? '',
            'timestamp' => $row['timestamp'] ?? null,
        ];
        if ($region === '' || $key === '') {
            return $raw;
        }

        try {
            $meta = self::fetchMeta($region, $key);
            $html = self::fetchHtml($region, $key);
            $raw['from'] = $meta['name'] ?? $raw['from'];
            $raw['to'] = $meta['recipients'] ?? $raw['to'];
            $raw['subject'] = $meta['subject'] ?? $raw['subject'];
            $raw['html'] = $html;
        } catch (\Throwable) {
            // 补全失败时退回列表元数据
        }
        return $raw;
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email): array
    {
        $local = explode('@', trim($email))[0];
        if ($local === '') {
            throw new \InvalidArgumentException('inboxkitten: empty email');
        }
        $resp = HttpClient::get(
            self::API_BASE . '/list?recipient=' . rawurlencode($local),
            self::HEADERS_JSON,
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('inboxkitten: 列表请求失败 ' . $resp->getStatusCode());
        }
        $rows = HttpClient::json($resp);
        if (!array_is_list($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $item) {
            if (is_array($item)) {
                $out[] = Normalize::email(self::detailRaw($item, $email), $email);
            }
        }
        return $out;
    }
}
