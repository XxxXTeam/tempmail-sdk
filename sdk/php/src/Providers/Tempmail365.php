<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * Tempmail365 渠道实现 — https://tempmail365.cn
 *
 * GET tempemail.php?action=get_config 拉域名（失败用后备）；
 * ?action=create_email 创建；?action=fetch_mail 收信（content 为 HTML，需正则提取）。
 */
final class Tempmail365
{
    private const CHANNEL = 'tempmail365';
    private const BASE = 'https://tempmail365.cn/tempemail.php';

    /** @var string[] 后备域名列表 */
    private const FALLBACK_DOMAINS = ['fengyou.cc', 'shop345.com', 'nutemail.com', 'qvrf.cn'];

    /** @var array<string,string> */
    private const HEADERS = [
        'Accept' => 'application/json, text/plain, */*',
        'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
        'Cache-Control' => 'no-cache',
        'Pragma' => 'no-cache',
        'Referer' => 'https://tempmail365.cn/',
        'User-Agent' => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0',
    ];

    /**
     * @return string[]
     */
    private static function fetchDomains(): array
    {
        try {
            $resp = HttpClient::get(self::BASE, self::HEADERS, query: ['action' => 'get_config']);
            $data = HttpClient::json($resp);
            $raw = $data['domains'] ?? null;
            if (!is_array($raw) || empty($raw)) {
                return self::FALLBACK_DOMAINS;
            }
            $out = [];
            foreach ($raw as $d) {
                if (is_string($d) && trim($d) !== '') {
                    $out[] = trim($d);
                }
            }
            return $out ?: self::FALLBACK_DOMAINS;
        } catch (\Throwable) {
            return self::FALLBACK_DOMAINS;
        }
    }

    private static function randomUsername(int $length = 8): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    private static function extractByLabel(string $content, string $cnLabel, string $enLabel): string
    {
        $pattern = '/(?:' . $cnLabel . '|' . $enLabel . ')\s*[:：]\s*(.+?)(?:<br|<\/|<p|\n|\r)/i';
        if (preg_match($pattern, $content, $m)) {
            return trim($m[1]);
        }
        return '';
    }

    public static function generate(?string $domain = null): EmailInfo
    {
        $domains = self::fetchDomains();
        if (empty($domains)) {
            throw new \RuntimeException('tempmail365: 无可用域名');
        }

        if ($domain !== null && trim($domain) !== '') {
            $d = strtolower(trim($domain));
            $selected = null;
            foreach ($domains as $x) {
                if (strtolower($x) === $d) {
                    $selected = $x;
                    break;
                }
            }
            if ($selected === null) {
                throw new \RuntimeException('tempmail365: 域名不可用: ' . $d);
            }
        } else {
            $selected = $domains[array_rand($domains)];
        }

        $addr = self::randomUsername() . '@' . $selected;
        $resp = HttpClient::get(self::BASE, self::HEADERS, query: [
            'action' => 'create_email',
            'email' => $addr,
            'domain' => $selected,
        ]);
        $data = HttpClient::json($resp);
        if (empty($data['success'])) {
            throw new \RuntimeException('tempmail365: 创建邮箱失败');
        }
        return new EmailInfo(self::CHANNEL, $addr);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token = null): array
    {
        $addr = trim($email);
        if ($addr === '') {
            throw new \InvalidArgumentException('tempmail365: 邮箱地址为空');
        }
        $resp = HttpClient::get(self::BASE, self::HEADERS, query: [
            'action' => 'fetch_mail',
            'email' => $addr,
        ]);
        $data = HttpClient::json($resp);
        $content = $data['content'] ?? null;
        if (!is_string($content) || $content === '' || trim($content) === '无邮件') {
            return [];
        }
        $raw = [
            'from' => self::extractByLabel($content, '发件人', 'From'),
            'subject' => self::extractByLabel($content, '主题', 'Subject'),
            'body' => $content,
            'date' => gmdate('c'),
        ];
        return [Normalize::email($raw, $addr)];
    }
}
