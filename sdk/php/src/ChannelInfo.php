<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 渠道信息（用于 listChannels / getChannelInfo 返回）
 */
final class ChannelInfo
{
    /**
     * @param string $channel 渠道标识
     * @param string $name    渠道显示名称
     * @param string $website 对应的临时邮箱服务网站
     */
    public function __construct(
        public string $channel = '',
        public string $name = '',
        public string $website = '',
    ) {
    }
}
