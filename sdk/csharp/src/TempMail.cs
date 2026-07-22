using System;
using System.Collections.Generic;
using System.Linq;

namespace XxxXTeam.TempMail;

/// <summary>
/// SDK 主入口：无状态函数式 API。聚合所有渠道，提供统一的创建/收信能力，支持渠道 fallback。
/// 所有 ListChannels / 渠道信息 / 两个 dispatch 均从注册表派生。
/// </summary>
public static class TempMail
{
    // 渠道标识数组缓存：随 Registry 快照引用变化而重建，避免每次 GenerateEmail 都 Select+ToArray 分配。
    private static IReadOnlyList<ChannelSpec>? _channelsSource;
    private static string[]? _allChannelsCache;
    private static readonly object _channelsLock = new();

    /// <summary>所有支持的渠道标识（有序，由注册表派生，带缓存）</summary>
    public static IReadOnlyList<string> AllChannels
    {
        get
        {
            var src = Registry.All;
            var cache = _allChannelsCache;
            if (cache is not null && ReferenceEquals(src, _channelsSource)) return cache;
            lock (_channelsLock)
            {
                if (_allChannelsCache is not null && ReferenceEquals(src, _channelsSource))
                    return _allChannelsCache;
                var arr = new string[src.Count];
                for (var i = 0; i < arr.Length; i++) arr[i] = src[i].Channel;
                _channelsSource = src;
                _allChannelsCache = arr;
                return arr;
            }
        }
    }

    /// <summary>获取所有支持的渠道列表（有序，与 baseline 逐行一致）</summary>
    public static List<ChannelInfo> ListChannels() =>
        Registry.All.Select(s => new ChannelInfo { Channel = s.Channel, Name = s.Name, Website = s.Website }).ToList();

    /// <summary>获取指定渠道的详细信息</summary>
    public static ChannelInfo? GetChannelInfo(string channel)
    {
        var spec = Registry.Lookup(channel);
        return spec is null ? null : new ChannelInfo { Channel = spec.Channel, Name = spec.Name, Website = spec.Website };
    }

    /// <summary>
    /// 创建临时邮箱。
    /// 指定渠道失败时自动打乱其余渠道逐个尝试；全部不可用时返回 null（不抛异常）。
    /// </summary>
    public static EmailInfo? GenerateEmail(GenerateEmailOptions? options = null)
    {
        options ??= new GenerateEmailOptions();
        var tryOrder = BuildChannelOrder(options.Channel);

        // 域名筛选
        var targetDomains = new List<string>();
        if (!string.IsNullOrEmpty(options.Suffix))
            targetDomains.Add(options.Suffix.TrimStart('@'));
        if (options.Domains is not null)
            targetDomains.AddRange(options.Domains);
        if (targetDomains.Count > 0)
            tryOrder = FilterChannelsByDomain(tryOrder, targetDomains);

        var maxChannels = options.MaxChannelsTried > 0 ? options.MaxChannelsTried : 20;
        var totalTimeout = options.TotalTimeout > 0 ? options.TotalTimeout : 60.0;
        var start = DateTimeOffset.UtcNow;

        var channelsTried = 0;
        var lastErr = "";
        foreach (var ch in tryOrder)
        {
            if (channelsTried >= maxChannels) break;
            if ((DateTimeOffset.UtcNow - start).TotalSeconds >= totalTimeout) break;

            channelsTried++;
            var r = Retry.WithAttempts(() => GenerateEmailOnce(ch, options), options.Retry);
            if (r.Ok)
            {
                Telemetry.Report("generate_email", ch, true, r.Attempts, channelsTried, "");
                return r.Value;
            }
            lastErr = r.Error?.Message ?? "unknown";
        }

        Telemetry.Report("generate_email", "", false, 0, channelsTried, lastErr);
        return null;
    }

    /// <summary>
    /// 获取邮件列表。Channel/Email/Token 由 SDK 从 EmailInfo 自动获取。
    /// 网络错误/超时/429/5xx 自动重试，重试耗尽后返回 Success=false。
    /// </summary>
    public static GetEmailsResult GetEmails(EmailInfo info, GetEmailsOptions? options = null)
    {
        if (info is null)
        {
            Telemetry.Report("get_emails", "", false, 0, 0, "EmailInfo is required, call GenerateEmail() first");
            throw new ArgumentNullException(nameof(info), "EmailInfo is required, call GenerateEmail() first");
        }

        var channel = info.Channel;
        var email = info.Email;
        var token = info.Token;

        if (string.IsNullOrEmpty(channel))
        {
            Telemetry.Report("get_emails", "", false, 0, 0, "channel is required");
            throw new ArgumentException("channel is required");
        }
        if (string.IsNullOrEmpty(email) && channel != "tempmail-lol")
        {
            Telemetry.Report("get_emails", channel, false, 0, 0, "email is required");
            throw new ArgumentException("email is required");
        }

        var r = Retry.WithAttempts(() => GetEmailsOnce(channel, email, token), options?.Retry);
        if (r.Ok)
        {
            Telemetry.Report("get_emails", channel, true, r.Attempts, 0, "");
            return new GetEmailsResult { Channel = channel, Email = email, Emails = r.Value!, Success = true };
        }

        var errS = r.Error?.Message ?? "unknown";
        Telemetry.Report("get_emails", channel, false, r.Attempts, 0, errS);
        return new GetEmailsResult { Channel = channel, Email = email, Emails = new List<Email>(), Success = false };
    }

    private static List<string> BuildChannelOrder(string? preferred)
    {
        var shuffled = AllChannels.ToList();
        var rand = Random.Shared; // 线程安全，免共享 Random 竞态
        for (var i = shuffled.Count - 1; i > 0; i--)
        {
            var j = rand.Next(i + 1);
            (shuffled[i], shuffled[j]) = (shuffled[j], shuffled[i]);
        }
        if (string.IsNullOrEmpty(preferred)) return shuffled;
        var rest = shuffled.Where(ch => ch != preferred).ToList();
        rest.Insert(0, preferred);
        return rest;
    }

    /// <summary>按目标域名筛选渠道：保留渠道 Website 命中任一目标域名者，无命中则退回原顺序</summary>
    private static List<string> FilterChannelsByDomain(List<string> channels, List<string> domains)
    {
        var wanted = domains.Select(d => d.TrimStart('@').ToLowerInvariant()).Where(d => d.Length > 0).ToHashSet();
        if (wanted.Count == 0) return channels;
        var filtered = channels.Where(ch =>
        {
            var info = GetChannelInfo(ch);
            if (info is null) return false;
            var site = info.Website.ToLowerInvariant();
            return wanted.Any(w => site.Contains(w));
        }).ToList();
        return filtered.Count > 0 ? filtered : channels;
    }

    private static EmailInfo GenerateEmailOnce(string channel, GenerateEmailOptions options)
    {
        var spec = Registry.Lookup(channel) ?? throw new ArgumentException($"Unknown channel: {channel}");
        return spec.Generate(options);
    }

    private static List<Email> GetEmailsOnce(string channel, string email, string? token)
    {
        var spec = Registry.Lookup(channel) ?? throw new ArgumentException($"Unknown channel: {channel}");
        return spec.GetEmails(email, token);
    }
}

/// <summary>
/// 临时邮箱客户端：封装邮箱创建与邮件获取的完整流程，自动管理 EmailInfo 与 token。
/// </summary>
public sealed class TempEmailClient
{
    private EmailInfo? _emailInfo;

    /// <summary>当前缓存的邮箱信息</summary>
    public EmailInfo? EmailInfo => _emailInfo;

    /// <summary>创建临时邮箱并缓存邮箱信息</summary>
    public EmailInfo? Generate(GenerateEmailOptions? options = null)
    {
        _emailInfo = TempMail.GenerateEmail(options);
        return _emailInfo;
    }

    /// <summary>获取当前邮箱的邮件列表，必须先调用 Generate()</summary>
    public GetEmailsResult GetEmails(GetEmailsOptions? options = null)
    {
        if (_emailInfo is null)
        {
            Telemetry.Report("get_emails", "", false, 0, 0, "No email generated. Call Generate() first");
            throw new InvalidOperationException("No email generated. Call Generate() first");
        }
        return TempMail.GetEmails(_emailInfo, options);
    }
}
