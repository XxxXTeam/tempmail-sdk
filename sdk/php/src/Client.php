<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * 临时邮箱客户端
 *
 * 封装邮箱创建与邮件获取的完整流程，自动管理邮箱信息与认证令牌，
 * 用户无需手动传递 token。
 */
final class Client
{
    private ?EmailInfo $emailInfo = null;

    /**
     * 创建临时邮箱并缓存邮箱信息。
     */
    public function generate(?GenerateEmailOptions $options = null): ?EmailInfo
    {
        $this->emailInfo = TempMail::generateEmail($options);
        return $this->emailInfo;
    }

    /**
     * 获取当前邮箱的邮件列表，必须先调用 generate()。
     */
    public function getEmails(?GetEmailsOptions $options = null): GetEmailsResult
    {
        if ($this->emailInfo === null) {
            Telemetry::report('get_emails', '', false, 0, 0, 'No email generated. Call generate() first');
            throw new \RuntimeException('No email generated. Call generate() first');
        }
        return TempMail::getEmails($this->emailInfo, $options);
    }

    /**
     * 获取当前缓存的邮箱信息。
     */
    public function emailInfo(): ?EmailInfo
    {
        return $this->emailInfo;
    }

    /**
     * 获取所有支持的渠道列表。
     *
     * @return ChannelInfo[]
     */
    public function listChannels(): array
    {
        return TempMail::listChannels();
    }
}
