<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 创建临时邮箱后返回的邮箱信息
 *
 * Token 等认证信息由 SDK 内部维护（private），不对外暴露，仅供 getEmails 分发时读取。
 */
final class EmailInfo
{
    /**
     * @param string          $channel   创建该邮箱所使用的渠道标识
     * @param string          $email     临时邮箱地址
     * @param string|null     $token     认证令牌（内部维护，不对外暴露）
     * @param int|null        $expiresAt 邮箱过期时间（毫秒时间戳）
     * @param string|int|null $createdAt 邮箱创建时间
     */
    public function __construct(
        public readonly string $channel,
        public readonly string $email,
        private readonly ?string $token = null,
        public readonly ?int $expiresAt = null,
        public readonly string|int|null $createdAt = null,
    ) {
    }

    /**
     * 读取内部令牌，仅供 SDK 内部分发使用。
     */
    public function getToken(): ?string
    {
        return $this->token;
    }
}
