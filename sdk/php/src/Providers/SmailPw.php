<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use GuzzleHttp\Psr7\Response;

/**
 * smail.pw 渠道实现（https://smail.pw）
 *
 * Remix/Flight loader，_root.data 返回序列化数据流。
 * generate POST /_root.data（intent=generate）建立 __session 会话并解析收件地址；
 * getEmails GET /_root.data 复用 __session，正则/JSON 解析邮件行，
 * 再逐封 GET /api/email/{id} 补 html 正文。
 */
final class SmailPw
{
    private const CHANNEL = 'smail-pw';
    private const ROOT_DATA_URL = 'https://smail.pw/_root.data';
    private const API_BASE = 'https://smail.pw/api/email/';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => '*/*',
        'accept-language' => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
        'cache-control' => 'no-cache',
        'dnt' => '1',
        'origin' => 'https://smail.pw',
        'pragma' => 'no-cache',
        'referer' => 'https://smail.pw/',
        'sec-ch-ua' => '"Chromium";v="146", "Not-A.Brand";v="24", "Microsoft Edge";v="146"',
        'sec-ch-ua-mobile' => '?0',
        'sec-ch-ua-platform' => '"Windows"',
        'sec-fetch-dest' => 'empty',
        'sec-fetch-mode' => 'cors',
        'sec-fetch-site' => 'same-origin',
    ];

    private static function sessionCookie(Response $resp): string
    {
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            if (preg_match('/__session=([^;]+)/', $sc, $m) === 1) {
                return '__session=' . $m[1];
            }
        }
        return '';
    }

    private static function extractInbox(string $text): string
    {
        if (preg_match('/"([a-z0-9][a-z0-9.-]*@smail\.pw)"/i', $text, $m) === 1) {
            return $m[1];
        }
        if (preg_match('/\b([a-z0-9][a-z0-9.-]*@smail\.pw)\b/i', $text, $m) === 1) {
            return $m[1];
        }
        return '';
    }

    /**
     * 解析 _root.data 文本中的邮件行（多种序列化格式）。
     *
     * @return array<int,array<string,mixed>>
     */
    private static function parseMails(string $text, string $recipient): array
    {
        $chunks = [];

        $reMail = '/"id","([^"]+)","to_address","([^"]*)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)/';
        $reg = [];
        if (preg_match_all($reMail, $text, $m1, PREG_SET_ORDER) !== false) {
            foreach ($m1 as $g) {
                $reg[] = self::mkRow($g[1], $g[2] !== '' ? $g[2] : $recipient, $g[3], $g[4], $g[5], (int) $g[6]);
            }
        }
        if ($reg !== []) {
            $chunks[] = $reg;
        }

        $reNoSubj = '/"id","([^"]+)","to_address","([^"]*)","from_name","([^"]*)","from_address","([^"]*)","subject","time",(\d+)/';
        $reg0 = [];
        if (preg_match_all($reNoSubj, $text, $m2, PREG_SET_ORDER) !== false) {
            foreach ($m2 as $g) {
                $reg0[] = self::mkRow($g[1], $g[2] !== '' ? $g[2] : $recipient, $g[3], $g[4], '', (int) $g[5]);
            }
        }
        if ($reg0 !== []) {
            $chunks[] = $reg0;
        }

        $reMail2 = '/"id","([^"]+)","from_name","([^"]*)","from_address","([^"]*)","subject","([^"]*)","time",(\d+)/';
        $reg2 = [];
        if (preg_match_all($reMail2, $text, $m3, PREG_SET_ORDER) !== false) {
            foreach ($m3 as $g) {
                $reg2[] = self::mkRow($g[1], $recipient, $g[2], $g[3], $g[4], (int) $g[5]);
            }
        }
        if ($reg2 !== []) {
            $chunks[] = $reg2;
        }

        $root = json_decode($text, true);
        if (is_array($root)) {
            $plain = [];
            self::walkPlainRowEmails($root, $recipient, $plain);
            if ($plain !== []) {
                $chunks[] = $plain;
            }
        }

        $flat = [];
        foreach ($chunks as $part) {
            foreach ($part as $row) {
                $flat[] = $row;
            }
        }
        return self::mergeById($flat);
    }

    /**
     * @return array<string,mixed>
     */
    private static function mkRow(string $id, string $to, string $fromName, string $fromAddr, string $subject, int $timeMs): array
    {
        return [
            'id' => $id,
            'to_address' => $to,
            'from_name' => $fromName,
            'from_address' => $fromAddr,
            'subject' => $subject,
            'date' => $timeMs,
            'text' => '',
            'html' => '',
            'attachments' => [],
        ];
    }

    /**
     * 递归遍历 JSON 结构，收集含 subject + time 的行对象。
     *
     * @param mixed                          $node
     * @param array<int,array<string,mixed>> $out
     */
    private static function walkPlainRowEmails(mixed $node, string $recipient, array &$out): void
    {
        if (!is_array($node)) {
            return;
        }
        if (array_is_list($node)) {
            foreach ($node as $el) {
                self::walkPlainRowEmails($el, $recipient, $out);
            }
            return;
        }
        $subj = $node['subject'] ?? null;
        if (is_string($subj)) {
            $tr = $node['time'] ?? null;
            $timeMs = null;
            if (is_int($tr)) {
                $timeMs = $tr;
            } elseif (is_float($tr) && !is_nan($tr)) {
                $timeMs = (int) $tr;
            } elseif (is_string($tr) && ctype_digit($tr)) {
                $timeMs = (int) $tr;
            }
            if ($timeMs !== null) {
                $out[] = [
                    'id' => (string) ($node['id'] ?? ''),
                    'to_address' => (string) ($node['to_address'] ?? $recipient),
                    'from_name' => (string) ($node['from_name'] ?? ''),
                    'from_address' => (string) ($node['from_address'] ?? ''),
                    'subject' => $subj,
                    'date' => $timeMs,
                    'text' => '',
                    'html' => '',
                    'attachments' => [],
                ];
            }
        }
        foreach ($node as $v) {
            if (is_array($v)) {
                self::walkPlainRowEmails($v, $recipient, $out);
            }
        }
    }

    /**
     * 按 id 去重（保留首个），无 id 时合成匿名 id。
     *
     * @param array<int,array<string,mixed>> $rows
     * @return array<int,array<string,mixed>>
     */
    private static function mergeById(array $rows): array
    {
        $byId = [];
        $anon = 0;
        foreach ($rows as $mail) {
            $mid = (string) ($mail['id'] ?? '');
            if ($mid === '') {
                $mid = '__smail_' . $anon . '_' . ($mail['date'] ?? '') . '_'
                    . substr((string) ($mail['subject'] ?? ''), 0, 48);
                $mail['id'] = $mid;
                $anon++;
            }
            if (!isset($byId[$mid])) {
                $byId[$mid] = $mail;
            }
        }
        return array_values($byId);
    }

    /**
     * @return array{text:string,html:string}
     */
    private static function fetchEmailBody(string $cookie, string $mailId): array
    {
        if ($mailId === '' || str_starts_with($mailId, '__smail_')) {
            return ['text' => '', 'html' => ''];
        }
        try {
            $headers = self::HEADERS;
            $headers['Cookie'] = $cookie;
            $headers['Accept'] = 'application/json';
            $resp = HttpClient::get(self::API_BASE . $mailId, $headers);
            if ($resp->getStatusCode() !== 200) {
                return ['text' => '', 'html' => ''];
            }
            $data = HttpClient::json($resp);
            $html = $data['body'] ?? '';
            return ['text' => '', 'html' => is_string($html) ? $html : ''];
        } catch (\Throwable) {
            return ['text' => '', 'html' => ''];
        }
    }

    public static function generate(): EmailInfo
    {
        $headers = self::HEADERS;
        $headers['Content-Type'] = 'application/x-www-form-urlencoded;charset=UTF-8';
        $resp = HttpClient::post(self::ROOT_DATA_URL, $headers, null, 'intent=generate');
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('smail-pw: 请求失败 ' . $resp->getStatusCode());
        }
        $cookie = self::sessionCookie($resp);
        if ($cookie === '') {
            throw new \RuntimeException('smail-pw: Failed to extract __session cookie');
        }
        $text = (string) $resp->getBody();
        $email = self::extractInbox($text);
        if ($email === '') {
            throw new \RuntimeException('smail-pw: Failed to parse inbox from response');
        }
        return new EmailInfo(self::CHANNEL, $email, $cookie);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('smail-pw: token 不能为空');
        }
        $headers = self::HEADERS;
        $headers['Cookie'] = $token;
        $resp = HttpClient::get(self::ROOT_DATA_URL, $headers);
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('smail-pw: 读信失败 ' . $resp->getStatusCode());
        }
        $rawList = self::parseMails((string) $resp->getBody(), $email);
        $out = [];
        foreach ($rawList as $r) {
            $item = Normalize::email($r, $email);
            if ($item->id !== '') {
                $body = self::fetchEmailBody($token, $item->id);
                if ($body['html'] !== '') {
                    $item->html = $body['html'];
                }
                if ($body['text'] !== '') {
                    $item->text = $body['text'];
                }
            }
            $out[] = $item;
        }
        return $out;
    }
}
