<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 单个渠道的注册规格（单一事实来源）
 *
 * 每新增一个渠道只需在注册表追加一处 register(new ChannelSpec(...))，
 * 渠道列表、信息映射、创建/收信分发全部由该结构自动派生。
 */
final class ChannelSpec
{
    /**
     * @param string                              $channel   渠道标识
     * @param string                              $name      渠道显示名称
     * @param string                              $website   对应的临时邮箱服务网站
     * @param callable(GenerateEmailOptions):EmailInfo $generate  创建邮箱实现
     * @param callable(string,?string):Email[]    $getEmails 获取邮件实现
     */
    public function __construct(
        public string $channel,
        public string $name,
        public string $website,
        public mixed $generate,
        public mixed $getEmails,
    ) {
    }
}
