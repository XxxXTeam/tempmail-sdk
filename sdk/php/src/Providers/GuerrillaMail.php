<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * GuerrillaMail 渠道实现（含全部镜像）
 *
 * guerrillamail.com / sharklasers.com / grr.la / spam4.me 等共用同一套 ajax.php API，
 * 仅 baseURL 不同。通过 makeGenerate/makeGetEmails 传入 channel 与 baseUrl 复用逻辑。
 */
final class GuerrillaMail
{
    /** @var array<string,string> */
    private const HEADERS = [
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36',
    ];

    /**
     * 生成指定镜像的 generate 闭包。
     *
     * @return callable():EmailInfo
     */
    public static function makeGenerate(string $channel, string $baseUrl): callable
    {
        return static function () use ($channel, $baseUrl): EmailInfo {
            $resp = HttpClient::get($baseUrl . '?f=get_email_address&lang=en', self::HEADERS);
            $data = HttpClient::json($resp);
            if (empty($data['email_addr']) || empty($data['sid_token'])) {
                throw new \RuntimeException($channel . ': 缺少 email_addr 或 sid_token');
            }
            $expiresAt = null;
            if (!empty($data['email_timestamp'])) {
                $expiresAt = ((int) $data['email_timestamp'] + 3600) * 1000;
            }
            return new EmailInfo($channel, (string) $data['email_addr'], (string) $data['sid_token'], expiresAt: $expiresAt);
        };
    }

    /**
     * 生成指定镜像的 getEmails 闭包。
     *
     * @return callable(string,?string):Email[]
     */
    public static function makeGetEmails(string $baseUrl): callable
    {
        return static function (string $email, ?string $token) use ($baseUrl): array {
            if ($token === null || $token === '') {
                throw new \InvalidArgumentException('guerrillamail: token 不能为空');
            }
            $resp = HttpClient::get($baseUrl . '?f=check_email&seq=0&sid_token=' . rawurlencode($token), self::HEADERS);
            $data = HttpClient::json($resp);
            $list = $data['list'] ?? [];
            if (!is_array($list)) {
                return [];
            }
            $out = [];
            foreach ($list as $item) {
                if (!is_array($item)) {
                    continue;
                }
                $body = (string) ($item['mail_body'] ?? '');
                if ($body === '' && !empty($item['mail_id'])) {
                    try {
                        $dr = HttpClient::get(
                            $baseUrl . '?f=fetch_email&sid_token=' . rawurlencode($token) . '&email_id=' . rawurlencode((string) $item['mail_id']),
                            self::HEADERS,
                        );
                        if ($dr->getStatusCode() < 400) {
                            $body = (string) (HttpClient::json($dr)['mail_body'] ?? '');
                        }
                    } catch (\Throwable) {
                    }
                }
                $text = $body !== '' ? trim(preg_replace('/<[^>]+>/', ' ', $body) ?? '') : (string) ($item['mail_excerpt'] ?? '');
                $text = trim(preg_replace('/\s+/', ' ', $text) ?? '');
                $out[] = Normalize::email([
                    'id' => $item['mail_id'] ?? '',
                    'from' => $item['mail_from'] ?? '',
                    'to' => $email,
                    'subject' => $item['mail_subject'] ?? '',
                    'text' => $text,
                    'html' => $body,
                    'date' => $item['mail_date'] ?? '',
                    'isRead' => ($item['mail_read'] ?? 0) == 1,
                ], $email);
            }
            return $out;
        };
    }
}
