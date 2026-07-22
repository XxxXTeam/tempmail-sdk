using System;
using System.Collections.Generic;

namespace XxxXTeam.TempMail;

/// <summary>
/// 单个渠道的注册规格（注册表范式的核心）。
/// 每新增一个渠道只需追加一处 Registry.Register(new ChannelSpec {...})，
/// 渠道列表、信息映射、创建/收信分发全部由该结构自动派生。
/// </summary>
public sealed class ChannelSpec
{
    /// <summary>渠道标识</summary>
    public required string Channel { get; init; }

    /// <summary>渠道显示名称</summary>
    public required string Name { get; init; }

    /// <summary>对应的临时邮箱服务网站</summary>
    public required string Website { get; init; }

    /// <summary>创建邮箱的实现</summary>
    public required Func<GenerateEmailOptions, EmailInfo> Generate { get; init; }

    /// <summary>获取邮件的实现（参数：邮箱地址、token）</summary>
    public required Func<string, string?, List<Email>> GetEmails { get; init; }
}

/// <summary>
/// 渠道注册表：单一事实来源。
/// 有序列表 + 字典，注册顺序即枚举顺序（硬约束，与 baseline 逐行一致）。
/// </summary>
public static class Registry
{
    private static readonly List<ChannelSpec> _ordered = new();
    private static readonly Dictionary<string, ChannelSpec> _map = new(StringComparer.Ordinal);
    private static readonly object _lock = new();
    // 只读快照缓存：注册仅在模块初始化期发生一次，之后 All 被高频读取（每次 GenerateEmail 走 fallback），
    // 缓存避免每次复制 279 元素数组；Register 时置空触发下次惰性重建。
    private static volatile ChannelSpec[]? _snapshot;

    /// <summary>有序渠道注册表（只读视图，快照缓存）</summary>
    public static IReadOnlyList<ChannelSpec> All
    {
        get
        {
            var snap = _snapshot;
            if (snap is not null) return snap;
            lock (_lock)
            {
                return _snapshot ??= _ordered.ToArray();
            }
        }
    }

    /// <summary>
    /// 注册一个渠道。重复注册同一标识视为编程错误，直接抛出。
    /// </summary>
    public static void Register(ChannelSpec spec)
    {
        lock (_lock)
        {
            if (_map.ContainsKey(spec.Channel))
                throw new InvalidOperationException($"duplicate channel registration: {spec.Channel}");
            _ordered.Add(spec);
            _map[spec.Channel] = spec;
            _snapshot = null; // 失效快照，下次 All 惰性重建

        }
    }

    /// <summary>按标识查找渠道规格</summary>
    public static ChannelSpec? Lookup(string channel)
    {
        lock (_lock) { return _map.TryGetValue(channel, out var s) ? s : null; }
    }
}
