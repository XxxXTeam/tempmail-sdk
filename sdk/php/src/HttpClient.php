<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

use GuzzleHttp\Client;
use GuzzleHttp\Cookie\CookieJar;
use GuzzleHttp\Psr7\Response;

/**
 * 共享 HTTP 客户端
 *
 * 所有 provider 通过本类发起 HTTP 请求，自动应用全局配置（代理、超时、SSL 等）。
 * 底层复用 Guzzle Client 实例，仅在配置版本变更时重建，保证连接复用。
 */
final class HttpClient
{
    private static ?Client $client = null;
    private static int $cachedVersion = -1;
    private static int $cachedTimeout = 15;

    /**
     * 生成随机 IPv4 地址（每段 1-254），用于伪造来源 IP 请求头。
     */
    private static function randomIpv4(): string
    {
        return implode('.', [
            random_int(1, 254),
            random_int(1, 254),
            random_int(1, 254),
            random_int(1, 254),
        ]);
    }

    /**
     * 生成一组伪造来源 IP 的请求头（同一 IP）。
     *
     * @return array<string,string>
     */
    private static function spoofHeaders(): array
    {
        $ip = self::randomIpv4();
        return [
            'X-Forwarded-For' => $ip,
            'X-Real-IP' => $ip,
            'X-Originating-IP' => $ip,
        ];
    }

    /**
     * 获取带全局配置的 Guzzle 客户端（内部缓存，仅在配置变更时重建）。
     */
    private static function client(): Client
    {
        $currentVersion = ConfigStore::version();
        if (self::$client !== null && self::$cachedVersion === $currentVersion) {
            return self::$client;
        }

        $config = ConfigStore::get();
        $options = [
            'http_errors' => false,
            'allow_redirects' => true,
            'verify' => !$config->insecure,
            'timeout' => $config->timeout,
            'connect_timeout' => $config->timeout,
        ];
        if ($config->proxy !== null && $config->proxy !== '') {
            $options['proxy'] = $config->proxy;
        }
        if (!empty($config->headers)) {
            $options['headers'] = $config->headers;
        }

        self::$client = new Client($options);
        self::$cachedVersion = $currentVersion;
        self::$cachedTimeout = $config->timeout;
        return self::$client;
    }

    /**
     * 将伪造来源 IP 请求头合并到用户 headers（用户值优先）。
     *
     * @param array<string,string> $headers
     * @return array<string,string>
     */
    private static function mergeSpoof(array $headers): array
    {
        return array_merge(self::spoofHeaders(), $headers);
    }

    /**
     * 发起 GET 请求。
     *
     * @param array<string,string>       $headers 请求头
     * @param array<string,mixed>|null   $query   查询参数
     */
    public static function get(string $url, array $headers = [], ?array $query = null, ?int $timeout = null, ?bool $allowRedirects = null, ?CookieJar $cookies = null): Response
    {
        $opts = ['headers' => self::mergeSpoof($headers)];
        if ($query !== null) {
            $opts['query'] = $query;
        }
        if ($timeout !== null) {
            $opts['timeout'] = $timeout;
        }
        if ($allowRedirects !== null) {
            $opts['allow_redirects'] = $allowRedirects;
        }
        if ($cookies !== null) {
            $opts['cookies'] = $cookies;
        }
        /** @var Response $resp */
        $resp = self::client()->request('GET', $url, $opts);
        return $resp;
    }

    /**
     * 发起 POST 请求。
     *
     * @param array<string,string>     $headers 请求头
     * @param array<mixed>|null        $json    JSON 请求体
     * @param string|null              $body    原始请求体（与 $json 互斥）
     * @param array<string,mixed>|null $form    表单请求体
     */
    public static function post(
        string $url,
        array $headers = [],
        ?array $json = null,
        ?string $body = null,
        ?array $form = null,
        ?int $timeout = null,
        ?CookieJar $cookies = null,
        ?bool $allowRedirects = null,
    ): Response {
        $opts = ['headers' => self::mergeSpoof($headers)];
        if ($json !== null) {
            $opts['json'] = $json;
        }
        if ($body !== null) {
            $opts['body'] = $body;
        }
        if ($form !== null) {
            $opts['form_params'] = $form;
        }
        if ($cookies !== null) {
            $opts['cookies'] = $cookies;
        }
        if ($timeout !== null) {
            $opts['timeout'] = $timeout;
        }
        if ($allowRedirects !== null) {
            $opts['allow_redirects'] = $allowRedirects;
        }
        /** @var Response $resp */
        $resp = self::client()->request('POST', $url, $opts);
        return $resp;
    }

    /**
     * 读取响应体并解析为关联数组；解析失败返回空数组。
     *
     * @return array<mixed>
     */
    public static function json(Response $resp): array
    {
        $text = (string) $resp->getBody();
        if ($text === '') {
            return [];
        }
        $data = json_decode($text, true);
        return is_array($data) ? $data : [];
    }
}
