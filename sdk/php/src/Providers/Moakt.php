<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * moakt.com 渠道实现（含全部镜像域名：drmail.in / teml.net / tmpeml.com 等）
 *
 * 纯 HTML 抓取：GET 语言首页 → POST /inbox 创建邮箱（捕获 302 中的 tm_session Cookie）
 * → GET /inbox 解析邮箱地址；列表解析 /{locale}/email/{uuid}，正文 GET .../html 解析 .email-body。
 * 凭证为 tm_session 等 Cookie，序列化（locale + Cookie 头）后存入 token。
 */
final class Moakt
{
    private const ORIGIN = 'https://www.moakt.com';
    private const TOK_PREFIX = 'mok1:';

    private const DEFAULT_UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    // 邮件域名正则：形如 example.com（用于区分 domain 参数是「语言 locale」还是「邮件域名」）
    private const MAIL_DOMAIN_RE = '/^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?(?:\.[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?)+$/i';

    /**
     * 生成指定渠道的 generate 闭包（不同镜像渠道复用同一逻辑，仅邮件域名不同）。
     *
     * @param string      $channel 渠道标识
     * @param string|null $domain  强制邮件域名（如 "drmail.in"）；null 表示随机域名
     * @return callable():EmailInfo
     */
    public static function makeGenerate(string $channel, ?string $domain): callable
    {
        return static fn (): EmailInfo => self::generate($channel, $domain);
    }

    /**
     * 生成 getEmails 闭包。
     *
     * @return callable(string,?string):Email[]
     */
    public static function makeGetEmails(): callable
    {
        return static fn (string $email, ?string $token): array => self::getEmails($email, $token);
    }

    public static function generate(string $channel, ?string $domain): EmailInfo
    {
        [$locale, $mailDomain] = self::requestParts($domain);
        $base = self::ORIGIN . '/' . $locale;
        $inbox = $base . '/inbox';

        // 首页请求：拿到初始 Cookie 与可用域名列表
        $r1 = HttpClient::get($base, self::pageHeaders($base));
        if ($r1->getStatusCode() >= 400) {
            throw new \RuntimeException('moakt: 首页请求失败 ' . $r1->getStatusCode());
        }
        $cookieHdr = self::mergeCookieHdr('', $r1);

        if ($mailDomain !== '') {
            if (!in_array($mailDomain, self::parseServerDomains((string) $r1->getBody()), true)) {
                throw new \RuntimeException('moakt: 不支持的域名 ' . $mailDomain);
            }
            $postForm = [
                'setemail' => '',
                'username' => self::randomLocal(),
                'domain' => $mailDomain,
                'preferred_domain' => '',
            ];
        } else {
            $postForm = ['random' => '1'];
        }

        // POST /inbox 创建邮箱；不跟随 302，以便从响应头捕获 tm_session
        $r2 = HttpClient::post(
            $inbox,
            self::pageHeaders($base) + [
                'Content-Type' => 'application/x-www-form-urlencoded',
                'Cookie' => $cookieHdr,
            ],
            form: $postForm,
            allowRedirects: false,
        );
        $cookieHdr = self::mergeCookieHdr($cookieHdr, $r2);

        if (!array_key_exists('tm_session', self::parseCookieMap($cookieHdr))) {
            throw new \RuntimeException('moakt: 缺少 tm_session cookie');
        }

        // GET /inbox 获取邮箱地址
        $r3 = HttpClient::get($inbox, self::pageHeaders($base) + ['Cookie' => $cookieHdr]);
        if ($r3->getStatusCode() >= 400) {
            throw new \RuntimeException('moakt: 收件箱请求失败 ' . $r3->getStatusCode());
        }
        $cookieHdr = self::mergeCookieHdr($cookieHdr, $r3);

        $email = self::parseInboxEmail((string) $r3->getBody());
        if ($mailDomain !== '' && self::emailDomain($email) !== $mailDomain) {
            throw new \RuntimeException(
                'moakt: 域名不匹配 expected=' . $mailDomain . ' actual=' . self::emailDomain($email)
            );
        }

        return new EmailInfo($channel, $email, self::encodeSess($locale, $cookieHdr));
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('moakt: token 不能为空');
        }
        [$locale, $cookieHdr] = self::decodeSess($token);
        $inbox = self::ORIGIN . '/' . $locale . '/inbox';
        $baseRef = self::ORIGIN . '/' . $locale;

        $r = HttpClient::get($inbox, self::pageHeaders($baseRef) + ['Cookie' => $cookieHdr]);
        if ($r->getStatusCode() >= 400) {
            throw new \RuntimeException('moakt: 收件箱请求失败 ' . $r->getStatusCode());
        }

        $out = [];
        foreach (self::listMailIds((string) $r->getBody()) as $mid) {
            $detail = self::ORIGIN . '/' . $locale . '/email/' . $mid . '/html';
            $refer = self::ORIGIN . '/' . $locale . '/email/' . $mid;
            $rd = HttpClient::get($detail, self::pageHeaders($refer) + ['Cookie' => $cookieHdr]);
            if ($rd->getStatusCode() !== 200) {
                continue;
            }
            $raw = self::parseDetail((string) $rd->getBody(), $mid, $email);
            $out[] = Normalize::email($raw, $email);
        }
        return $out;
    }

    /**
     * 解析 domain 参数：返回 [locale, mailDomain]。
     * - 空或含非法字符：默认 locale=zh，无邮件域名
     * - 形如域名：locale=zh，邮件域名为该值
     * - 其他：视为 locale 值
     *
     * @return array{0:string,1:string}
     */
    private static function requestParts(?string $domain): array
    {
        $s = trim((string) $domain);
        if ($s === '' || preg_match('#[/?\#\\\\]#', $s) === 1) {
            return ['zh', ''];
        }
        if (preg_match(self::MAIL_DOMAIN_RE, $s) === 1) {
            return ['zh', strtolower(ltrim($s, '@'))];
        }
        return [$s, ''];
    }

    /**
     * 解析首页 <option value="..."> @domain </option> 得到可用域名集合。
     *
     * @return string[]
     */
    private static function parseServerDomains(string $page): array
    {
        $out = [];
        if (preg_match_all('/<option\s+value="([^"]+)">\s*@[^<]+<\/option>/is', $page, $ms) !== false) {
            foreach ($ms[1] as $v) {
                $v = strtolower(ltrim(trim($v), '@'));
                if ($v !== '') {
                    $out[$v] = true;
                }
            }
        }
        return array_keys($out);
    }

    private static function randomLocal(int $length = 12): string
    {
        $chars = 'abcdefghijklmnopqrstuvwxyz0123456789';
        $out = '';
        for ($i = 0; $i < $length; $i++) {
            $out .= $chars[random_int(0, strlen($chars) - 1)];
        }
        return $out;
    }

    private static function emailDomain(string $email): string
    {
        $pos = strrpos($email, '@');
        if ($pos === false) {
            return '';
        }
        return strtolower(trim(substr($email, $pos + 1)));
    }

    /**
     * 从当前用户代理配置解析 UA，缺省用内置 Chrome UA。
     */
    private static function userAgent(): string
    {
        $headers = \ChanhanzhanX\TempMail\ConfigStore::get()->headers;
        foreach ($headers as $k => $v) {
            if (strtolower((string) $k) === 'user-agent' && $v !== '') {
                return (string) $v;
            }
        }
        return self::DEFAULT_UA;
    }

    /**
     * @return array<string,string>
     */
    private static function pageHeaders(string $referer): array
    {
        return [
            'User-Agent' => self::userAgent(),
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6',
            'Cache-Control' => 'no-cache',
            'DNT' => '1',
            'Pragma' => 'no-cache',
            'Upgrade-Insecure-Requests' => '1',
            'Referer' => $referer,
        ];
    }

    /**
     * 解析 Cookie 头字符串为映射。
     *
     * @return array<string,string>
     */
    private static function parseCookieMap(string $hdr): array
    {
        $m = [];
        foreach (explode(';', $hdr) as $part) {
            $part = trim($part);
            if ($part === '' || !str_contains($part, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $part, 2);
            $k = trim($k);
            if ($k !== '') {
                $m[$k] = trim($v);
            }
        }
        return $m;
    }

    /**
     * 将响应 Set-Cookie 合并进已有 Cookie 头（键排序后重建）。
     */
    private static function mergeCookieHdr(string $prev, \Psr\Http\Message\ResponseInterface $resp): string
    {
        $d = self::parseCookieMap($prev);
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            // Set-Cookie 首段即 name=value
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first === '' || !str_contains($first, '=')) {
                continue;
            }
            [$k, $v] = explode('=', $first, 2);
            $k = trim($k);
            if ($k !== '') {
                $d[$k] = trim($v);
            }
        }
        ksort($d);
        $parts = [];
        foreach ($d as $k => $v) {
            $parts[] = $k . '=' . $v;
        }
        return implode('; ', $parts);
    }

    private static function encodeSess(string $locale, string $cookieHdr): string
    {
        $raw = json_encode(['l' => $locale, 'c' => $cookieHdr], JSON_UNESCAPED_SLASHES);
        return self::TOK_PREFIX . base64_encode((string) $raw);
    }

    /**
     * @return array{0:string,1:string}
     */
    private static function decodeSess(string $tok): array
    {
        if (!str_starts_with($tok, self::TOK_PREFIX)) {
            throw new \InvalidArgumentException('moakt: 无效的会话 token');
        }
        $decoded = base64_decode(substr($tok, strlen(self::TOK_PREFIX)), true);
        if ($decoded === false) {
            throw new \InvalidArgumentException('moakt: 无效的会话 token');
        }
        $o = json_decode($decoded, true);
        if (!is_array($o)) {
            throw new \InvalidArgumentException('moakt: 无效的会话 token');
        }
        $loc = trim((string) ($o['l'] ?? ''));
        $c = trim((string) ($o['c'] ?? ''));
        if ($loc === '' || $c === '') {
            throw new \InvalidArgumentException('moakt: 无效的会话 token');
        }
        return [$loc, $c];
    }

    private static function parseInboxEmail(string $html): string
    {
        if (preg_match('/<div\s+id="email-address"\s*>([^<]+)<\/div>/is', $html, $m) !== 1) {
            throw new \RuntimeException('moakt: 未找到 email-address');
        }
        $addr = trim(html_entity_decode($m[1], ENT_QUOTES | ENT_HTML5));
        if ($addr === '') {
            throw new \RuntimeException('moakt: email-address 为空');
        }
        return $addr;
    }

    /**
     * 从收件箱页解析邮件 UUID 列表（去重，排除删除链接）。
     *
     * @return string[]
     */
    private static function listMailIds(string $html): array
    {
        $seen = [];
        $out = [];
        $re = '#href="(/[^"]+/email/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})"#';
        if (preg_match_all($re, $html, $ms) !== false) {
            foreach ($ms[1] as $path) {
                if (str_contains($path, '/delete')) {
                    continue;
                }
                $mid = substr($path, strrpos($path, '/') + 1);
                if (strlen($mid) === 36 && !isset($seen[$mid])) {
                    $seen[$mid] = true;
                    $out[] = $mid;
                }
            }
        }
        return $out;
    }

    /**
     * 解析邮件详情页，提取发件人、主题、日期与正文 HTML。
     *
     * @return array<string,string>
     */
    private static function parseDetail(string $page, string $mid, string $recipient): array
    {
        $from = '';
        if (preg_match('/<li\s+class="sender"[^>]*>[\s\S]*?<span[^>]*>([\s\S]*?)<\/span>\s*<\/li>/is', $page, $sm) === 1) {
            $inner = html_entity_decode($sm[1], ENT_QUOTES | ENT_HTML5);
            $from = trim(preg_replace('/<[^>]+>/', ' ', $inner) ?? $inner);
            if (preg_match('/<([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})>/', $inner, $em) === 1) {
                $from = trim($em[1]);
            }
        }

        $subject = '';
        if (preg_match('/<li\s+class="title"\s*>([^<]*)<\/li>/is', $page, $tm) === 1) {
            $subject = trim(html_entity_decode($tm[1], ENT_QUOTES | ENT_HTML5));
        }

        $date = '';
        if (preg_match('/<li\s+class="date"[^>]*>[\s\S]*?<span[^>]*>([^<]+)<\/span>/is', $page, $dm) === 1) {
            $date = trim(html_entity_decode($dm[1], ENT_QUOTES | ENT_HTML5));
        }

        $body = '';
        if (preg_match('/<div\s+class="email-body"\s*>([\s\S]*?)<\/div>/is', $page, $bm) === 1) {
            $body = trim($bm[1]);
        }

        return [
            'id' => $mid,
            'to' => $recipient,
            'from' => $from,
            'subject' => $subject,
            'date' => $date,
            'html' => $body,
        ];
    }
}
