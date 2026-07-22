<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;

/**
 * mjj-cm 渠道实现（Socket.IO 临时邮箱）
 *
 * 共用 SocketIoMail 协议，主机为 mjj.cm。
 */
final class MjjCm
{
    private const CHANNEL = 'mjj-cm';
    private const HOST = 'mjj.cm';

    public static function generate(): EmailInfo
    {
        return SocketIoMail::generate(self::CHANNEL, self::HOST);
    }

    /**
     * @return Email[]
     */
    public static function getEmails(string $email, ?string $token): array
    {
        return SocketIoMail::getEmails(self::CHANNEL, $email, $token);
    }
}
