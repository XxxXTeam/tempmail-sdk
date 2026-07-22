package com.xxxxteam.tempmail;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ThreadLocalRandom;

/**
 * SDK 主入口：无状态函数式 API。
 *
 * <p>聚合所有渠道，提供统一的创建/收信能力，支持渠道 fallback。
 * 所有 {@code listChannels} / 渠道信息 / 两个 dispatch 均从注册表派生。</p>
 */
public final class TempMail {

    static {
        // 首次加载主入口类时按环境变量 TEMPMAIL_WEBUI 决定是否启动可视化面板（幂等）
        WebUI.maybeStartFromEnv();
    }

    private TempMail() {
    }

    /**
     * 所有支持的渠道标识（有序，由注册表派生）。
     *
     * @return 渠道标识列表
     */
    public static List<String> allChannels() {
        List<String> result = new ArrayList<>();
        for (ChannelSpec s : Registry.all()) {
            result.add(s.getChannel());
        }
        return result;
    }

    /**
     * 获取所有支持的渠道列表（有序，与 baseline 逐行一致）。
     *
     * @return 渠道信息列表
     */
    public static List<ChannelInfo> listChannels() {
        List<ChannelInfo> result = new ArrayList<>();
        for (ChannelSpec s : Registry.all()) {
            result.add(new ChannelInfo(s.getChannel(), s.getName(), s.getWebsite()));
        }
        return result;
    }

    /**
     * 获取指定渠道的详细信息。
     *
     * @param channel 渠道标识
     * @return 渠道信息，未知渠道返回 null
     */
    public static ChannelInfo getChannelInfo(String channel) {
        ChannelSpec spec = Registry.lookup(channel);
        return spec == null ? null : new ChannelInfo(spec.getChannel(), spec.getName(), spec.getWebsite());
    }

    /**
     * 使用默认选项创建临时邮箱。
     *
     * @return 邮箱信息，全部渠道不可用时返回 null
     */
    public static EmailInfo generateEmail() {
        return generateEmail(new GenerateEmailOptions());
    }

    /**
     * 创建临时邮箱。
     *
     * <p>指定渠道失败时自动打乱其余渠道逐个尝试；全部不可用时返回 null（不抛异常）。</p>
     *
     * @param options 创建选项，可为 null
     * @return 邮箱信息，全部渠道不可用时返回 null
     */
    public static EmailInfo generateEmail(GenerateEmailOptions options) {
        final GenerateEmailOptions opts = options != null ? options : new GenerateEmailOptions();
        List<String> tryOrder = buildChannelOrder(opts.getChannel());

        // 域名筛选
        List<String> targetDomains = new ArrayList<>();
        if (opts.getSuffix() != null && !opts.getSuffix().isEmpty()) {
            String s = opts.getSuffix();
            targetDomains.add(s.startsWith("@") ? s.substring(1) : s);
        }
        if (opts.getDomains() != null) {
            targetDomains.addAll(opts.getDomains());
        }
        if (!targetDomains.isEmpty()) {
            tryOrder = filterChannelsByDomain(tryOrder, targetDomains);
        }

        int maxChannels = opts.getMaxChannelsTried() > 0 ? opts.getMaxChannelsTried() : 20;
        long totalTimeoutMillis = opts.getTotalTimeoutMillis() > 0 ? opts.getTotalTimeoutMillis() : 60000L;
        long start = System.currentTimeMillis();

        int channelsTried = 0;
        String lastErr = "";
        for (String ch : tryOrder) {
            if (channelsTried >= maxChannels) {
                break;
            }
            if (System.currentTimeMillis() - start >= totalTimeoutMillis) {
                break;
            }
            channelsTried++;
            final String channel = ch;
            Retry.Result<EmailInfo> r = Retry.withAttempts(
                    () -> generateEmailOnce(channel, opts), opts.getRetry());
            if (r.isOk()) {
                Telemetry.report("generate_email", ch, true, r.getAttempts(), channelsTried, "");
                WebUI.log("info", ch, "创建邮箱成功: " + r.getValue().getEmail());
                return r.getValue();
            }
            lastErr = r.getError() != null ? String.valueOf(r.getError().getMessage()) : "unknown";
            WebUI.log("warn", ch, "创建邮箱失败，尝试下一个渠道: " + lastErr);
        }

        Telemetry.report("generate_email", "", false, 0, channelsTried, lastErr);
        WebUI.log("error", "", "全部渠道创建邮箱失败，共尝试 " + channelsTried + " 个渠道");
        return null;
    }

    /**
     * 使用默认选项获取邮件列表。
     *
     * @param info 邮箱信息
     * @return 收信结果
     */
    public static GetEmailsResult getEmails(EmailInfo info) {
        return getEmails(info, null);
    }

    /**
     * 获取邮件列表。Channel/Email/Token 由 SDK 从 {@link EmailInfo} 自动获取。
     *
     * <p>网络错误/超时/429/5xx 自动重试，重试耗尽后返回 success=false。</p>
     *
     * @param info    邮箱信息
     * @param options 收信选项，可为 null
     * @return 收信结果
     */
    public static GetEmailsResult getEmails(EmailInfo info, GetEmailsOptions options) {
        if (info == null) {
            Telemetry.report("get_emails", "", false, 0, 0,
                    "EmailInfo is required, call generateEmail() first");
            throw new IllegalArgumentException("EmailInfo is required, call generateEmail() first");
        }

        String channel = info.getChannel();
        String email = info.getEmail();
        String token = info.getToken();

        if (channel == null || channel.isEmpty()) {
            Telemetry.report("get_emails", "", false, 0, 0, "channel is required");
            throw new IllegalArgumentException("channel is required");
        }
        if ((email == null || email.isEmpty()) && !"tempmail-lol".equals(channel)) {
            Telemetry.report("get_emails", channel, false, 0, 0, "email is required");
            throw new IllegalArgumentException("email is required");
        }

        final String ch = channel;
        final String em = email;
        final String tok = token;
        RetryConfig retry = options != null ? options.getRetry() : null;
        WebUI.log("debug", ch, "开始收信: " + em);
        Retry.Result<List<Email>> r = Retry.withAttempts(() -> getEmailsOnce(ch, em, tok), retry);
        if (r.isOk()) {
            Telemetry.report("get_emails", channel, true, r.getAttempts(), 0, "");
            WebUI.log("info", channel, "收信成功，共 " + r.getValue().size() + " 封");
            return new GetEmailsResult(channel, email, r.getValue(), true);
        }

        String errS = r.getError() != null ? String.valueOf(r.getError().getMessage()) : "unknown";
        Telemetry.report("get_emails", channel, false, r.getAttempts(), 0, errS);
        WebUI.log("error", channel, "收信失败: " + errS);
        return new GetEmailsResult(channel, email, new ArrayList<>(), false);
    }

    private static List<String> buildChannelOrder(String preferred) {
        List<String> shuffled = allChannels();
        for (int i = shuffled.size() - 1; i > 0; i--) {
            int j = ThreadLocalRandom.current().nextInt(i + 1);
            String tmp = shuffled.get(i);
            shuffled.set(i, shuffled.get(j));
            shuffled.set(j, tmp);
        }
        if (preferred == null || preferred.isEmpty()) {
            return shuffled;
        }
        List<String> rest = new ArrayList<>();
        for (String ch : shuffled) {
            if (!ch.equals(preferred)) {
                rest.add(ch);
            }
        }
        rest.add(0, preferred);
        return rest;
    }

    /**
     * 按目标域名筛选渠道：保留渠道 Website 命中任一目标域名者，无命中则退回原顺序。
     */
    private static List<String> filterChannelsByDomain(List<String> channels, List<String> domains) {
        Set<String> wanted = new HashSet<>();
        for (String d : domains) {
            String w = d.startsWith("@") ? d.substring(1) : d;
            w = w.toLowerCase();
            if (!w.isEmpty()) {
                wanted.add(w);
            }
        }
        if (wanted.isEmpty()) {
            return channels;
        }
        List<String> filtered = new ArrayList<>();
        for (String ch : channels) {
            ChannelInfo info = getChannelInfo(ch);
            if (info == null) {
                continue;
            }
            String site = info.getWebsite().toLowerCase();
            for (String w : wanted) {
                if (site.contains(w)) {
                    filtered.add(ch);
                    break;
                }
            }
        }
        return !filtered.isEmpty() ? filtered : channels;
    }

    private static EmailInfo generateEmailOnce(String channel, GenerateEmailOptions options) throws Exception {
        ChannelSpec spec = Registry.lookup(channel);
        if (spec == null) {
            throw new IllegalArgumentException("Unknown channel: " + channel);
        }
        return spec.getGenerate().generate(options);
    }

    private static List<Email> getEmailsOnce(String channel, String email, String token) throws Exception {
        ChannelSpec spec = Registry.lookup(channel);
        if (spec == null) {
            throw new IllegalArgumentException("Unknown channel: " + channel);
        }
        return spec.getGetEmails().getEmails(email, token);
    }
}
