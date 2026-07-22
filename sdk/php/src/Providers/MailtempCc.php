<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * MailTemp.cc 渠道实现 — https://mailtemp.cc
 *
 * PHP POST form 接口，无鉴权：POST /api.php action=inbox 获取用户名（JSON 字符串）；
 * token 存用户名，地址为 {username}@neocea.com；
 * action=fetch 拉列表，action=view 拉单封详情。
 */
final class MailtempCc
{
    private const BASE = 'https://mailtemp.cc';
    private const DOMAIN = 'neocea.com';

    /** @var array<string,string> */
    private const HEADERS = [
        'Content-Type' => 'application/x-www-form-urlencoded',
        'Accept' => '*/*',
        'Referer' => self::BASE . '/',
        'Origin' => self::BASE,
    ];

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::post(self::BASE . '/api.php', self::HEADERS, body: 'action=inbox');
        $rawText = trim((string) $resp->getBody());
        // 返回值为 JSON 字符串格式（带引号），如 "vindictiverate"
        $username = json_decode($rawText, true);
        if (!is_string($username) || $username === '') {
            $username = $rawText;
        }
        if (!is_string($username) || $username === '') {
            throw new \RuntimeException('mailtemp-cc: 返回的用户名无效');
        }
        $address = $username . '@' . self::DOMAIN;
        return new EmailInfo('mailtemp-cc', $address, $username);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('mailtemp-cc: token 不能为空');
        }
        $address = trim($email);
        if ($address === '') {
            throw new \InvalidArgumentException('mailtemp-cc: 邮箱地址不能为空');
        }

        $resp = HttpClient::post(
            self::BASE . '/api.php',
            self::HEADERS,
            body: 'action=fetch&inbox=' . $token . '&last_id=0',
        );
        $rawList = HttpClient::json($resp);
        if (!array_is_list($rawList)) {
            return [];
        }

        $out = [];
        foreach ($rawList as $item) {
            if (!is_array($item)) {
                continue;
            }
            $mailId = $item['id'] ?? '';
            if ($mailId === '' || $mailId === null) {
                continue;
            }
            $detailResp = HttpClient::post(
                self::BASE . '/api.php',
                self::HEADERS,
                body: 'action=view&id=' . $mailId . '&inbox=' . $token,
            );
            $detail = HttpClient::json($detailResp);
            if (!is_array($detail)) {
                $detail = [];
            }
            $raw = [
                'id' => (string) $mailId,
                'from' => $item['sender_email'] ?? ($item['sender'] ?? ''),
                'to' => $address,
                'subject' => $item['subject'] ?? ($detail['subject'] ?? ''),
                'text' => '',
                'html' => $detail['body_html'] ?? '',
                'date' => $item['received_at'] ?? ($detail['received_at'] ?? ''),
            ];
            $out[] = Normalize::email($raw, $address);
        }
        return $out;
    }
}
