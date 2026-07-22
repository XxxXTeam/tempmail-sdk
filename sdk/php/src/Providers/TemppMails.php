<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;

/**
 * tempp-mails.com 渠道实现（Laravel + CSRF，与 tempmailten.com 共享模板）— https://tempp-mails.com
 *
 * 流程与 tempmailten 完全一致：GET /en 取 CSRF 与 session cookie →
 * POST /messages 取邮箱与列表 → GET /view/{id} 取正文。
 * 复用 TempMailTen 的参数化核心逻辑，仅渠道标识、域名与 token 前缀不同。
 */
final class TemppMails
{
    private const CHANNEL = 'tempp-mails';
    private const BASE_URL = 'https://tempp-mails.com';
    private const TOK_PREFIX = 'tpm1:';

    public static function generate(): EmailInfo
    {
        return TempMailTen::doGenerate(self::CHANNEL, self::BASE_URL, self::TOK_PREFIX);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        return TempMailTen::doGetEmails($email, $token, self::CHANNEL, self::BASE_URL, self::TOK_PREFIX);
    }
}
