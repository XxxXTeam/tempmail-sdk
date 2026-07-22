<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * SDK 全局配置
 *
 * 包含代理、超时、SSL、遥测等设置，作用于所有 HTTP 请求。
 * 支持环境变量自动读取（优先级低于代码设置）：
 *   TEMPMAIL_PROXY              - 代理 URL（http/https/socks5）
 *   TEMPMAIL_TIMEOUT            - 超时秒数
 *   TEMPMAIL_INSECURE           - "1"/"true" 跳过 SSL 验证
 *   DROPMAIL_AUTH_TOKEN         - DropMail GraphQL af_ 令牌
 *   APIHZ_ID / APIHZ_KEY        - apihz（接口盒子）凭据
 *   TEMPMAIL_TELEMETRY_ENABLED  - false/0/no 关闭匿名上报
 *   TEMPMAIL_TELEMETRY_URL      - 自定义上报端点
 */
final class Config
{
    /**
     * @param string|null           $proxy            代理 URL
     * @param int                   $timeout          全局默认超时秒数
     * @param bool                  $insecure         跳过 SSL 证书验证
     * @param array<string,string>  $headers          自定义请求头
     * @param string|null           $dropmailAuthToken DropMail af_ 令牌
     * @param string|null           $apihzId          apihz 调用 id
     * @param string|null           $apihzKey         apihz 调用 key
     * @param bool|null             $telemetryEnabled null=默认开启；false=关闭
     * @param string|null           $telemetryUrl     自定义上报端点
     */
    public function __construct(
        public ?string $proxy = null,
        public int $timeout = 15,
        public bool $insecure = false,
        public array $headers = [],
        public ?string $dropmailAuthToken = null,
        public ?string $apihzId = null,
        public ?string $apihzKey = null,
        public ?bool $telemetryEnabled = null,
        public ?string $telemetryUrl = null,
    ) {
    }

    /**
     * 从环境变量构造默认配置。
     */
    public static function fromEnv(): self
    {
        $proxy = getenv('TEMPMAIL_PROXY') ?: null;

        $timeoutStr = getenv('TEMPMAIL_TIMEOUT');
        $timeout = ($timeoutStr !== false && ctype_digit($timeoutStr)) ? (int) $timeoutStr : 15;

        $insecureVal = (string) (getenv('TEMPMAIL_INSECURE') ?: '');
        $insecure = in_array(strtolower($insecureVal), ['1', 'true', 'yes'], true);

        $dmTok = trim((string) (getenv('DROPMAIL_AUTH_TOKEN') ?: getenv('DROPMAIL_API_TOKEN') ?: '')) ?: null;

        $teRaw = strtolower(trim((string) (getenv('TEMPMAIL_TELEMETRY_ENABLED') ?: '')));
        $telemetryEnabled = null;
        if (in_array($teRaw, ['false', '0', 'no'], true)) {
            $telemetryEnabled = false;
        } elseif (in_array($teRaw, ['true', '1', 'yes'], true)) {
            $telemetryEnabled = true;
        }

        $tu = trim((string) (getenv('TEMPMAIL_TELEMETRY_URL') ?: '')) ?: null;
        $apihzId = trim((string) (getenv('APIHZ_ID') ?: '')) ?: null;
        $apihzKey = trim((string) (getenv('APIHZ_KEY') ?: '')) ?: null;

        return new self(
            proxy: $proxy,
            timeout: $timeout,
            insecure: $insecure,
            dropmailAuthToken: $dmTok,
            apihzId: $apihzId,
            apihzKey: $apihzKey,
            telemetryEnabled: $telemetryEnabled,
            telemetryUrl: $tu,
        );
    }
}
