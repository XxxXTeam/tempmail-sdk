<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail\Providers;

use ChanhanzhanX\TempMail\Email;
use ChanhanzhanX\TempMail\EmailInfo;

/**
 * 桩渠道实现
 *
 * 用于 HTML 抓取 / WebSocket / socket.io 等尚未落地真实逻辑的复杂渠道。
 * 保证渠道可正常注册与枚举，被实际调用时抛出未实现异常。
 */
final class Stub
{
    /**
     * 生成指定渠道的 generate 桩闭包。
     *
     * @return callable():EmailInfo
     */
    public static function makeGenerate(string $channel): callable
    {
        return static function () use ($channel): EmailInfo {
            throw new \RuntimeException('渠道 ' . $channel . ' 暂未实现 generate（桩实现）');
        };
    }

    /**
     * 生成指定渠道的 getEmails 桩闭包（返回空列表，避免打断轮询）。
     *
     * @return callable(string,?string):Email[]
     */
    public static function makeGetEmails(string $channel): callable
    {
        return static function (string $email, ?string $token) use ($channel): array {
            throw new \RuntimeException('渠道 ' . $channel . ' 暂未实现 getEmails（桩实现）');
        };
    }
}
