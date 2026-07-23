<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Mailinator 渠道实现（含全部姊妹域名）
 *
 * API: https://mailinator.com
 * 主渠道使用 mailinator.com 域名；大量姊妹域名 MX 均指向 mail.mailinator.com，
 * 收信统一复用 domain=public 的公共收件箱 API（可读所有姊妹域名邮件）。
 * 通过 makeGenerate/makeGetEmails 工厂复用同一套逻辑，仅生成邮箱时的域名不同。
 */
final class Mailinator
{
    private const BASE_URL = 'https://mailinator.com';
    private const PUBLIC_DOMAIN = 'public';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
        'Cache-Control' => 'no-cache',
        'Pragma' => 'no-cache',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    /**
     * 生成随机本地部分（小写字母数字）。
     */
    private static function randomString(int $length): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * 生成指定渠道/域名的 generate 闭包。
     *
     * @param string $channel 渠道标识
     * @param string $domain  邮箱域名（主渠道为 mailinator.com，姊妹渠道为各自域名）
     * @return callable():EmailInfo
     */
    public static function makeGenerate(string $channel, string $domain): callable
    {
        return static function () use ($channel, $domain): EmailInfo {
            $local = self::randomString(12);
            return new EmailInfo($channel, $local . '@' . $domain);
        };
    }

    /**
     * 生成 getEmails 闭包（所有渠道共用，收信复用 domain=public 公共收件箱）。
     *
     * @return callable(string,?string):Email[]
     */
    public static function makeGetEmails(): callable
    {
        return static function (string $email, ?string $token): array {
            return self::getEmails($email, $token);
        };
    }

    /**
     * 拉取收件箱邮件：从邮箱地址取本地部分作为收件箱名，逐封拉取正文/HTML/附件。
     *
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        unset($token);
        $inbox = trim($email);
        if (str_contains($inbox, '@')) {
            $inbox = explode('@', $inbox, 2)[0];
        }
        if ($inbox === '') {
            return [];
        }

        $data = self::requestJson(
            self::BASE_URL . '/api/v2/domains/' . self::PUBLIC_DOMAIN . '/inboxes/' . rawurlencode($inbox)
        );
        $messages = self::parseMessages($data);
        if (empty($messages)) {
            return [];
        }

        $result = [];
        foreach ($messages as $msg) {
            $messageId = trim((string) ($msg['id'] ?? $msg['messageId'] ?? ''));
            $textPayload = null;
            $htmlPayload = null;
            $attachmentsPayload = null;
            if ($messageId !== '') {
                $base = self::BASE_URL . '/api/v2/domains/' . self::PUBLIC_DOMAIN . '/messages/' . rawurlencode($messageId);
                $textPayload = self::requestJson($base . '/text');
                $htmlPayload = self::requestJson($base . '/texthtml');
                $attachmentsPayload = self::requestJson($base . '/attachments');
            }
            $result[] = Normalize::email(
                self::flatten($msg, $email, $textPayload, $htmlPayload, $attachmentsPayload),
                $email
            );
        }
        return $result;
    }

    /**
     * 发起 GET 并解析 JSON；失败返回 null。
     *
     * @return array<mixed>|null
     */
    private static function requestJson(string $url): ?array
    {
        try {
            $resp = HttpClient::get($url, self::HEADERS, timeout: 15);
            if ($resp->getStatusCode() >= 400) {
                return null;
            }
            return HttpClient::json($resp);
        } catch (\Throwable) {
            return null;
        }
    }

    /**
     * 从收件箱响应中提取消息数组，兼容纯数组与 {msgs|data:[...]}。
     *
     * @param array<mixed>|null $data
     * @return array<int,array<mixed>>
     */
    private static function parseMessages(?array $data): array
    {
        if ($data === null) {
            return [];
        }
        $candidate = null;
        if (array_is_list($data)) {
            $candidate = $data;
        } else {
            foreach (['msgs', 'data'] as $key) {
                if (isset($data[$key]) && is_array($data[$key]) && array_is_list($data[$key])) {
                    $candidate = $data[$key];
                    break;
                }
            }
        }
        if ($candidate === null) {
            return [];
        }
        $out = [];
        foreach ($candidate as $item) {
            if (is_array($item)) {
                $out[] = $item;
            }
        }
        return $out;
    }

    /**
     * 从正文 payload 中按 key 提取文本内容。
     *
     * @param array<mixed>|null $payload
     */
    private static function textFromPayload(?array $payload, string $key): string
    {
        if ($payload === null) {
            return '';
        }
        $value = $payload[$key] ?? null;
        if ($value === null) {
            return '';
        }
        return is_string($value) ? $value : (is_scalar($value) ? (string) $value : '');
    }

    /**
     * 补全附件下载地址（相对路径补上 BASE_URL）。
     */
    private static function attachmentUrl(mixed $value): string
    {
        if (!is_string($value) || $value === '') {
            return '';
        }
        if (str_starts_with($value, 'http://') || str_starts_with($value, 'https://')) {
            return $value;
        }
        return self::BASE_URL . $value;
    }

    /**
     * 将消息摘要与正文/附件 payload 扁平化为 normalize 可识别的字段。
     *
     * @param array<mixed>      $summary
     * @param array<mixed>|null $textPayload
     * @param array<mixed>|null $htmlPayload
     * @param array<mixed>|null $attachmentsPayload
     * @return array<mixed>
     */
    private static function flatten(
        array $summary,
        string $recipient,
        ?array $textPayload,
        ?array $htmlPayload,
        ?array $attachmentsPayload,
    ): array {
        $attachments = [];
        $rawAtt = $attachmentsPayload['attachments'] ?? null;
        if (is_array($rawAtt)) {
            foreach ($rawAtt as $att) {
                if (!is_array($att)) {
                    continue;
                }
                $attachments[] = [
                    'filename' => $att['name'] ?? $att['filename'] ?? '',
                    'size' => $att['size'] ?? $att['filesize'] ?? null,
                    'contentType' => $att['contentType'] ?? $att['content_type'] ?? $att['mimeType'] ?? $att['mime_type'] ?? null,
                    'url' => self::attachmentUrl($att['downloadUrl'] ?? $att['url'] ?? null),
                ];
            }
        }

        // 优先读 "text" 键（/text 端点实际返回键），回退兜底 "text/plain"（防御性编程）
        $textContent = self::textFromPayload($textPayload, 'text');
        if ($textContent === '') {
            $textContent = self::textFromPayload($textPayload, 'text/plain');
        }
        // 优先读 "html" 键，回退兜底 "text/html"
        $htmlContent = self::textFromPayload($htmlPayload, 'html');
        if ($htmlContent === '') {
            $htmlContent = self::textFromPayload($htmlPayload, 'text/html');
        }

        return [
            'id' => $summary['id'] ?? $summary['messageId'] ?? '',
            'from' => $summary['from'] ?? $summary['origfrom'] ?? '',
            'to' => $summary['to'] ?? $recipient,
            'subject' => $summary['subject'] ?? '',
            'text' => $textContent,
            'html' => $htmlContent,
            'date' => $summary['time'] ?? $summary['date'] ?? '',
            'seen' => false,
            'attachments' => $attachments,
        ];
    }
}
