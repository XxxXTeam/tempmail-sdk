<?php

declare(strict_types=1);

namespace ChanhanzhanX\TempMail;

/**
 * SDK 主入口（无状态函数式 API）
 *
 * 聚合全部渠道逻辑，提供统一的创建邮箱、获取邮件、枚举渠道能力。
 * 所有能力均从 Registry 派生，新增渠道只需在注册表追加一处。
 */
final class TempMail
{
    /**
     * 入口引导：按需启动 WebUI 服务器（受环境变量控制，仅生效一次）。
     * 在每个对外方法首行调用，确保任意入口都会触发自动检查。
     */
    private static function bootstrap(): void
    {
        WebUI::maybeStart();
    }

    /**
     * 获取所有支持的渠道列表（有序，与 baseline 逐行一致）。
     *
     * @return ChannelInfo[]
     */
    public static function listChannels(): array
    {
        self::bootstrap();
        $out = [];
        foreach (Registry::all() as $spec) {
            $out[] = new ChannelInfo($spec->channel, $spec->name, $spec->website);
        }
        return $out;
    }

    /**
     * 获取指定渠道的详细信息。
     */
    public static function getChannelInfo(string $channel): ?ChannelInfo
    {
        self::bootstrap();
        $spec = Registry::get($channel);
        return $spec === null ? null : new ChannelInfo($spec->channel, $spec->name, $spec->website);
    }

    /**
     * 创建临时邮箱
     *
     * 错误处理策略：
     * - 指定渠道失败时，自动打乱其余渠道逐个尝试
     * - 未指定渠道时，打乱全部渠道逐个尝试，直到成功
     * - 所有渠道均不可用时返回 null（不抛异常）
     */
    public static function generateEmail(?GenerateEmailOptions $options = null): ?EmailInfo
    {
        self::bootstrap();
        $options ??= new GenerateEmailOptions();

        $tryOrder = self::buildChannelOrder($options->channel);

        // 域名筛选
        $targetDomains = [];
        if ($options->suffix !== null && $options->suffix !== '') {
            $targetDomains[] = ltrim($options->suffix, '@');
        }
        if ($options->domains !== null) {
            foreach ($options->domains as $d) {
                $targetDomains[] = $d;
            }
        }
        if (!empty($targetDomains)) {
            $tryOrder = self::filterChannelsByDomain($tryOrder, $targetDomains);
        }

        $maxChannels = $options->maxChannelsTried > 0 ? $options->maxChannelsTried : 20;
        $totalTimeout = $options->totalTimeout > 0 ? $options->totalTimeout : 60.0;
        $startTime = microtime(true);

        $channelsTried = 0;
        $lastErr = '';
        foreach ($tryOrder as $ch) {
            if ($channelsTried >= $maxChannels) {
                Logger::warning('已尝试最大渠道数 (' . $maxChannels . ')，停止');
                break;
            }
            if (microtime(true) - $startTime >= $totalTimeout) {
                Logger::warning('整体超时，停止尝试');
                break;
            }

            $spec = Registry::get($ch);
            if ($spec === null) {
                continue;
            }

            $channelsTried++;
            Logger::info('创建临时邮箱, 渠道: ' . $ch);
            $r = Retry::withAttempts(
                static fn (): EmailInfo => ($spec->generate)($options),
                $options->retry,
            );
            if ($r->ok && $r->value instanceof EmailInfo) {
                Logger::info('邮箱创建成功: ' . $r->value->email . ' (渠道: ' . $ch . ')');
                Telemetry::report('generate_email', $ch, true, $r->attempts, $channelsTried, '');
                return $r->value;
            }
            $lastErr = $r->error !== null ? $r->error->getMessage() : 'unknown';
            Logger::warning('渠道 ' . $ch . ' 不可用: ' . $lastErr . '，尝试下一个渠道');
        }

        Logger::error('所有渠道均不可用，创建邮箱失败');
        Telemetry::report('generate_email', '', false, 0, $channelsTried, $lastErr);
        return null;
    }

    /**
     * 获取邮件列表
     *
     * Channel/Email/Token 等由 SDK 从 EmailInfo 自动获取，用户无需手动传递。
     */
    public static function getEmails(EmailInfo $info, ?GetEmailsOptions $options = null): GetEmailsResult
    {
        self::bootstrap();
        $channel = $info->channel;
        $email = $info->email;
        $token = $info->getToken();
        $retry = $options?->retry;

        if ($channel === '') {
            Telemetry::report('get_emails', '', false, 0, 0, 'channel is required');
            throw new \InvalidArgumentException('channel is required');
        }
        if ($email === '' && $channel !== 'tempmail-lol') {
            Telemetry::report('get_emails', $channel, false, 0, 0, 'email is required');
            throw new \InvalidArgumentException('email is required');
        }

        $spec = Registry::get($channel);
        if ($spec === null) {
            throw new \InvalidArgumentException('Unknown channel: ' . $channel);
        }

        $r = Retry::withAttempts(
            static fn (): array => ($spec->getEmails)($email, $token),
            $retry,
        );
        if ($r->ok && is_array($r->value)) {
            $count = count($r->value);
            if ($count > 0) {
                Logger::info('获取到 ' . $count . ' 封邮件, 渠道: ' . $channel);
            }
            Telemetry::report('get_emails', $channel, true, $r->attempts, 0, '');
            return new GetEmailsResult($channel, $email, $r->value, true);
        }

        $errS = $r->error !== null ? $r->error->getMessage() : 'unknown';
        Logger::error('获取邮件失败, 渠道: ' . $channel . ', 错误: ' . $errS);
        Telemetry::report('get_emails', $channel, false, $r->attempts, 0, $errS);
        return new GetEmailsResult($channel, $email, [], false);
    }

    /**
     * 构建渠道尝试顺序：指定渠道优先，其余打乱追加；未指定则打乱全部。
     *
     * @return string[]
     */
    private static function buildChannelOrder(?string $preferred): array
    {
        $all = Registry::channelNames();
        shuffle($all);
        if ($preferred === null || $preferred === '') {
            return $all;
        }
        $rest = array_values(array_filter($all, static fn (string $c): bool => $c !== $preferred));
        return array_merge([$preferred], $rest);
    }

    /**
     * 按目标域名筛选渠道（当前实现保持全部渠道，域名匹配由 provider 内部处理）。
     *
     * @param string[] $channels
     * @param string[] $domains
     * @return string[]
     */
    private static function filterChannelsByDomain(array $channels, array $domains): array
    {
        // 简化处理：域名筛选交由支持 domain 参数的 provider 自行匹配，此处不预先裁剪。
        return $channels;
    }
}
