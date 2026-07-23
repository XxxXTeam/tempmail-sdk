<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;

/**
 * Maildrop 渠道实现
 *
 * https://maildrop.cx，GET /api/suffixes.php 拉取域名，GET /api/emails.php 拉取邮件。
 */
final class Maildrop
{
    private const BASE = 'https://maildrop.cx';
    private const EXCLUDED_SUFFIX = 'transformer.edu.kg';

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json, text/plain, */*',
        'Referer' => 'https://maildrop.cx/zh-cn/app',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
        'x-requested-with' => 'XMLHttpRequest',
    ];

    private static function randomLocal(int $length = 10): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    /**
     * @return string[]
     */
    private static function fetchSuffixes(): array
    {
        $resp = HttpClient::get(self::BASE . '/api/suffixes.php', self::HEADERS);
        $data = HttpClient::json($resp);
        $ex = strtolower(self::EXCLUDED_SUFFIX);
        $out = [];
        foreach ($data as $x) {
            if (is_string($x)) {
                $t = trim($x);
                if ($t !== '' && strtolower($t) !== $ex) {
                    $out[] = $t;
                }
            }
        }
        if (empty($out)) {
            throw new \RuntimeException('maildrop: 无可用域名');
        }
        return $out;
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        $suffixes = self::fetchSuffixes();
        if ($domain !== null && trim($domain) !== '') {
            $p = strtolower(trim($domain));
            $dom = null;
            foreach ($suffixes as $d) {
                if (strtolower($d) === $p) {
                    $dom = $d;
                    break;
                }
            }
            if ($dom === null) {
                throw new \RuntimeException('maildrop: 域名不可用: ' . $p);
            }
        } else {
            $dom = $suffixes[array_rand($suffixes)];
        }
        $email = self::randomLocal() . '@' . $dom;
        return new EmailInfo('maildrop', $email, $email);
    }

    /**
     * 通过详情接口获取单封邮件完整内容
     * GET /api/email_content.php?id={id}
     * 详情响应字段（从前端代码确认）:
     *   - content: 完整 HTML 正文
     *   - subject / from_addr / date: 邮件元数据
     *   - attachment: JSON 字符串数组 [{filename, path, size}]（可能为空）
     *
     * @return array<string,mixed>|null 详情数组，失败返回 null
     */
    private static function fetchDetail(string $id): ?array
    {
        $mid = trim($id);
        if ($mid === '') {
            return null;
        }
        try {
            $resp = HttpClient::get(self::BASE . '/api/email_content.php', self::HEADERS, query: ['id' => $mid]);
            $status = $resp->getStatusCode();
            if ($status < 200 || $status >= 300) {
                return null;
            }
            $data = HttpClient::json($resp);
            return is_array($data) ? $data : null;
        } catch (\Throwable $e) {
            return null;
        }
    }

    /**
     * 解析详情接口的 attachment 字段（JSON 字符串）为附件数组
     *
     * @return array<int,array<string,mixed>>
     */
    private static function parseAttachments(mixed $raw): array
    {
        if (!is_string($raw) || trim($raw) === '') {
            return [];
        }
        $items = json_decode($raw, true);
        if (!is_array($items)) {
            return [];
        }
        $out = [];
        foreach ($items as $it) {
            if (!is_array($it)) {
                continue;
            }
            $filename = trim((string) ($it['filename'] ?? ''));
            if ($filename === '') {
                continue;
            }
            $entry = ['filename' => $filename];
            if (is_int($it['size'] ?? null) || is_float($it['size'] ?? null)) {
                $entry['size'] = (int) $it['size'];
            }
            $out[] = $entry;
        }
        return $out;
    }

    /**
     * 获取邮件列表并对每封邮件补拉详情。
     * 1. GET /api/emails.php 拉取列表（仅含 description 摘要）
     * 2. 对每封邮件 GET /api/email_content.php?id={id} 拉取详情（含 content 完整 HTML）
     * 3. 详情失败时保留列表 description 作为回退
     *
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $addr = trim($email) !== '' ? trim($email) : trim((string) $token);
        if ($addr === '') {
            throw new \InvalidArgumentException('maildrop: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::BASE . '/api/emails.php', self::HEADERS, query: [
            'addr' => $addr,
            'page' => '1',
            'limit' => '20',
        ]);
        $data = HttpClient::json($resp);
        $rows = $data['emails'] ?? [];
        if (!is_array($rows)) {
            return [];
        }
        $out = [];
        foreach ($rows as $row) {
            if (!is_array($row)) {
                continue;
            }
            $mailId = (string) ($row['id'] ?? '');
            $desc = trim((string) ($row['description'] ?? ''));
            $ir = $row['isRead'] ?? null;

            $from = trim((string) ($row['from_addr'] ?? ''));
            $subject = trim((string) ($row['subject'] ?? ''));
            $date = self::cxDateToIso((string) ($row['date'] ?? ''));
            $html = '';
            $attachments = [];

            /* 拉取详情覆盖 html/from/subject/date/attachments */
            if ($mailId !== '') {
                $detail = self::fetchDetail($mailId);
                if ($detail !== null) {
                    $dContent = $detail['content'] ?? null;
                    if (is_string($dContent) && trim($dContent) !== '') {
                        $html = $dContent;
                    }
                    $dFrom = $detail['from_addr'] ?? null;
                    if (is_string($dFrom) && trim($dFrom) !== '') {
                        $from = trim($dFrom);
                    }
                    $dSubj = $detail['subject'] ?? null;
                    if (is_string($dSubj) && trim($dSubj) !== '') {
                        $subject = trim($dSubj);
                    }
                    $dDate = $detail['date'] ?? null;
                    if (is_string($dDate) && trim($dDate) !== '') {
                        $date = self::cxDateToIso($dDate);
                    }
                    $attachments = self::parseAttachments($detail['attachment'] ?? null);
                }
            }

            $out[] = new Email(
                id: $mailId,
                fromAddr: $from,
                to: $addr,
                subject: $subject,
                text: $desc,
                html: $html,
                date: $date,
                isRead: $ir === true || $ir === 1,
                attachments: $attachments,
            );
        }
        return $out;
    }

    private static function cxDateToIso(string $s): string
    {
        $s = trim($s);
        if (strlen($s) === 19 && $s[10] === ' ') {
            return substr($s, 0, 10) . 'T' . substr($s, 11) . 'Z';
        }
        return $s;
    }
}
