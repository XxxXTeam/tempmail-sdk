<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\ConfigStore;
use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;
use ChanhanzhanX\TempMail\HttpClient;
use ChanhanzhanX\TempMail\Normalize;

/**
 * DropMail 渠道实现（dropmail.me GraphQL）
 *
 * 需要 af_ 认证令牌：优先取配置/环境变量（DROPMAIL_AUTH_TOKEN），
 * 否则自动向 token/generate 申请并进程内缓存。
 * generate: mutation introduceSession；getEmails: query session(id){mails}。
 */
final class DropMail
{
    private const TOKEN_GENERATE_URL = 'https://dropmail.me/api/token/generate';

    private const CREATE_SESSION_QUERY =
        'mutation {introduceSession {id, expiresAt, addresses{id, address}}}';

    private const GET_MAILS_QUERY = 'query ($id: ID!) {
  session(id:$id) {
    mails {
      id, rawSize, fromAddr, toAddr, receivedAt,
      text, headerFrom, headerSubject, html
    }
  }
}';

    /** @var array<string,string> */
    private const TOKEN_HEADERS = [
        'Accept' => 'application/json',
        'Content-Type' => 'application/json',
        'Origin' => 'https://dropmail.me',
        'Referer' => 'https://dropmail.me/api/',
    ];

    private const AUTO_CACHE_SEC = 50 * 60;

    private static ?string $cachedAf = null;
    private static float $cachedAfExpiry = 0.0;

    /**
     * 读取显式配置的 af_ 令牌（配置优先，其次环境变量）。
     */
    private static function explicitAfToken(): string
    {
        $cfg = ConfigStore::get();
        $t = trim((string) ($cfg->dropmailAuthToken ?? ''));
        if ($t !== '') {
            return $t;
        }
        return trim((string) (getenv('DROPMAIL_AUTH_TOKEN') ?: getenv('DROPMAIL_API_TOKEN') ?: ''));
    }

    /**
     * 向 token/generate 申请新的 af_ 令牌。
     */
    private static function fetchAfToken(): string
    {
        $resp = HttpClient::post(
            self::TOKEN_GENERATE_URL,
            self::TOKEN_HEADERS,
            body: json_encode(['type' => 'af', 'lifetime' => '1h']),
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('dropmail: token/generate 失败 ' . $resp->getStatusCode());
        }
        $body = HttpClient::json($resp);
        $tok = trim((string) ($body['token'] ?? ''));
        if ($tok === '' || !str_starts_with($tok, 'af_')) {
            throw new \RuntimeException((string) ($body['error'] ?? 'DropMail token/generate 未返回有效 af_ 令牌'));
        }
        return $tok;
    }

    /**
     * 解析可用的 af_ 令牌：显式配置优先，否则使用/刷新进程内缓存。
     */
    private static function resolveAfToken(): string
    {
        $ex = self::explicitAfToken();
        if ($ex !== '') {
            return $ex;
        }
        $now = microtime(true);
        if (self::$cachedAf !== null && $now < self::$cachedAfExpiry - 600) {
            return self::$cachedAf;
        }
        $tok = self::fetchAfToken();
        self::$cachedAf = $tok;
        self::$cachedAfExpiry = microtime(true) + self::AUTO_CACHE_SEC;
        return $tok;
    }

    private static function graphqlUrl(): string
    {
        return 'https://dropmail.me/api/graphql/' . rawurlencode(self::resolveAfToken());
    }

    /**
     * 发送 GraphQL 请求（form-urlencoded），返回 data 段。
     *
     * @param array<mixed>|null $variables
     * @return array<mixed>
     */
    private static function graphqlRequest(string $query, ?array $variables = null): array
    {
        $form = ['query' => $query];
        if ($variables !== null) {
            $form['variables'] = json_encode($variables);
        }
        $resp = HttpClient::post(
            self::graphqlUrl(),
            ['Content-Type' => 'application/x-www-form-urlencoded'],
            form: $form,
        );
        if ($resp->getStatusCode() >= 400) {
            throw new \RuntimeException('dropmail: graphql 失败 ' . $resp->getStatusCode());
        }
        $result = HttpClient::json($resp);
        if (!empty($result['errors'])) {
            $msg = $result['errors'][0]['message'] ?? 'Unknown';
            throw new \RuntimeException('dropmail: GraphQL error: ' . $msg);
        }
        return is_array($result['data'] ?? null) ? $result['data'] : [];
    }

    public static function generate(): EmailInfo
    {
        $data = self::graphqlRequest(self::CREATE_SESSION_QUERY);
        $session = $data['introduceSession'] ?? null;
        if (!is_array($session) || empty($session['addresses'][0]['address'])) {
            throw new \RuntimeException('dropmail: Failed to generate email');
        }
        return new EmailInfo(
            'dropmail',
            (string) $session['addresses'][0]['address'],
            (string) $session['id'],
            createdAt: $session['expiresAt'] ?? null,
        );
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        if ($token === null || $token === '') {
            throw new \InvalidArgumentException('dropmail: token 不能为空');
        }
        $data = self::graphqlRequest(self::GET_MAILS_QUERY, ['id' => $token]);
        $mails = $data['session']['mails'] ?? [];
        if (!is_array($mails)) {
            return [];
        }
        $out = [];
        foreach ($mails as $m) {
            if (!is_array($m)) {
                continue;
            }
            $out[] = Normalize::email([
                'id' => $m['id'] ?? '',
                'from' => $m['fromAddr'] ?? '',
                'to' => $m['toAddr'] ?? $email,
                'subject' => $m['headerSubject'] ?? '',
                'text' => $m['text'] ?? '',
                'html' => $m['html'] ?? '',
                'received_at' => $m['receivedAt'] ?? '',
                'attachments' => [],
            ], $email);
        }
        return $out;
    }
}
