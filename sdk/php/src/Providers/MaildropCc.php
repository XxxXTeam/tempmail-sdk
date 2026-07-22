<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * maildrop.cc 渠道实现（https://maildrop.cc）
 *
 * GraphQL API，无认证，公共邮箱（任何人可查看任意地址收件箱）。
 * generate 无需 API，直接随机用户名 + @maildrop.cc；
 * getEmails POST https://api.maildrop.cc/graphql，先 inbox 查列表，再逐封 message 补 html 正文。
 */
final class MaildropCc
{
    private const CHANNEL = 'maildrop-cc';
    private const DOMAIN = 'maildrop.cc';
    private const GRAPHQL_URL = 'https://api.maildrop.cc/graphql';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json',
        'Content-Type' => 'application/json',
        'Origin' => 'https://maildrop.cc',
        'Referer' => 'https://maildrop.cc/',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
            . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    private const LOCAL_CHARS = 'abcdefghijklmnopqrstuvwxyz0123456789';

    private static function randomLocal(int $n = 10): string
    {
        $out = '';
        for ($i = 0; $i < $n; $i++) {
            $out .= self::LOCAL_CHARS[random_int(0, strlen(self::LOCAL_CHARS) - 1)];
        }
        return $out;
    }

    private static function mailbox(string $email): string
    {
        $email = trim($email);
        $idx = strpos($email, '@');
        return $idx !== false ? substr($email, 0, $idx) : $email;
    }

    /**
     * 发送 GraphQL 请求并返回解析后的 JSON。
     *
     * @return array<mixed>
     */
    private static function doGraphql(string $query): array
    {
        $resp = HttpClient::post(self::GRAPHQL_URL, self::HEADERS, ['query' => $query]);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('maildrop-cc: GraphQL 请求失败 ' . $resp->getStatusCode());
        }
        return HttpClient::json($resp);
    }

    private static function inboxQuery(string $mailbox): string
    {
        $mb = json_encode($mailbox);
        return 'query { inbox(mailbox: ' . $mb . ') { id headerfrom subject date } }';
    }

    private static function messageQuery(string $mailbox, string $mid): string
    {
        $mb = json_encode($mailbox);
        $midS = json_encode($mid);
        return 'query { message(mailbox: ' . $mb . ', id: ' . $midS . ') { id headerfrom subject date html } }';
    }

    public static function generate(): EmailInfo
    {
        $email = self::randomLocal(10) . '@' . self::DOMAIN;
        return new EmailInfo(self::CHANNEL, $email, $email);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $mailbox = self::mailbox($email);
        if ($mailbox === '') {
            throw new \InvalidArgumentException('maildrop-cc: 邮箱地址为空');
        }

        $inboxResp = self::doGraphql(self::inboxQuery($mailbox));
        $items = $inboxResp['data']['inbox'] ?? null;
        if (!is_array($items) || $items === []) {
            return [];
        }

        $out = [];
        foreach ($items as $item) {
            if (!is_array($item)) {
                continue;
            }
            $mid = (string) ($item['id'] ?? '');
            $raw = [
                'id' => $mid,
                'from' => $item['headerfrom'] ?? '',
                'subject' => $item['subject'] ?? '',
                'date' => $item['date'] ?? '',
            ];
            if ($mid !== '') {
                try {
                    $msgResp = self::doGraphql(self::messageQuery($mailbox, $mid));
                    $msg = $msgResp['data']['message'] ?? null;
                    if (is_array($msg)) {
                        $raw = [
                            'id' => $msg['id'] ?? $mid,
                            'from' => $msg['headerfrom'] ?? '',
                            'subject' => $msg['subject'] ?? '',
                            'date' => $msg['date'] ?? '',
                            'html' => $msg['html'] ?? '',
                        ];
                    }
                } catch (\Throwable) {
                }
            }
            $out[] = Normalize::email($raw, $email);
        }
        return $out;
    }
}
