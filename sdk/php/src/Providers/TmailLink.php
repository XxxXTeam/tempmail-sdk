<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;
use Psr\Http\Message\ResponseInterface;

/**
 * tmail.link 渠道实现（Django CSRF）— https://tmail.link
 *
 * 创建: GET / 首页正则提取邮箱地址（xxx@tmail.link）→ GET /inbox/{email}/ 取 Set-Cookie 中的 csrftoken；
 * 读信: POST /inbox/{email}/（form: format=json + csrfmiddlewaretoken={token}，Cookie: csrftoken={token}）。
 * token 存储 csrftoken 值。
 */
final class TmailLink
{
    private const CHANNEL = 'tmail-link';
    private const BASE_URL = 'https://tmail.link';

    private const UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
        . '(KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0';

    /**
     * @return array<string,string>
     */
    private static function browserHeaders(): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
        ];
    }

    /**
     * @return array<string,string>
     */
    private static function postHeaders(string $email, string $token): array
    {
        return [
            'User-Agent' => self::UA,
            'Accept' => 'application/json, text/javascript, */*; q=0.01',
            'Accept-Language' => 'zh-CN,zh;q=0.9,en;q=0.8',
            'X-Requested-With' => 'XMLHttpRequest',
            'Content-Type' => 'application/x-www-form-urlencoded; charset=UTF-8',
            'Origin' => self::BASE_URL,
            'Referer' => self::BASE_URL . '/inbox/' . $email . '/',
            'Cookie' => 'csrftoken=' . $token,
        ];
    }

    public static function generate(): EmailInfo
    {
        $resp = HttpClient::get(self::BASE_URL . '/', self::browserHeaders());
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('tmail-link: 首页请求失败 ' . $resp->getStatusCode());
        }
        if (preg_match('/([a-zA-Z0-9._%+-]+@tmail\.link)/', (string) $resp->getBody(), $m) !== 1) {
            throw new \RuntimeException('tmail-link: 未能从首页提取邮箱地址');
        }
        $email = trim($m[1]);
        if ($email === '') {
            throw new \RuntimeException('tmail-link: 提取的邮箱地址为空');
        }

        $resp2 = HttpClient::get(self::BASE_URL . '/inbox/' . $email . '/', self::browserHeaders());
        if ($resp2->getStatusCode() >= 400) {
            throw new \RuntimeException('tmail-link: inbox 请求失败 ' . $resp2->getStatusCode());
        }
        $token = self::extractCsrfToken($resp2);
        if ($token === '') {
            throw new \RuntimeException('tmail-link: 未能获取 csrftoken');
        }

        return new EmailInfo(self::CHANNEL, $email, $token);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        $email = trim($email);
        if ($email === '') {
            throw new \InvalidArgumentException('tmail-link: 邮箱地址为空');
        }
        $tok = trim((string) $token);

        $resp = HttpClient::post(
            self::BASE_URL . '/inbox/' . $email . '/',
            self::postHeaders($email, $tok),
            form: ['format' => 'json', 'csrfmiddlewaretoken' => $tok],
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('tmail-link: 邮件列表请求失败 ' . $resp->getStatusCode());
        }
        $result = HttpClient::json($resp);
        $messages = $result['messages'] ?? null;
        if (!is_array($messages) || empty($messages)) {
            return [];
        }

        $out = [];
        foreach ($messages as $item) {
            if (!is_array($item)) {
                continue;
            }
            $out[] = Normalize::email([
                'id' => $item['key'] ?? '',
                'from' => $item['sender'] ?? '',
                'to' => $email,
                'subject' => $item['subject'] ?? '',
                'date' => $item['date'] ?? '',
            ], $email);
        }
        return $out;
    }

    /**
     * 从响应 Set-Cookie 中提取 csrftoken 值。
     */
    private static function extractCsrfToken(ResponseInterface $resp): string
    {
        foreach ($resp->getHeader('Set-Cookie') as $sc) {
            $first = trim(explode(';', $sc, 2)[0]);
            if ($first !== '' && str_contains($first, '=')) {
                [$k, $v] = explode('=', $first, 2);
                if (trim($k) === 'csrftoken') {
                    return trim($v);
                }
            }
        }
        return '';
    }
}
