package com.xxxxteam.tempmail;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * 渠道注册表：单一事实来源。
 *
 * <p>有序列表 + 映射，注册顺序即枚举顺序（硬约束，与 baseline 逐行一致）。
 * Java 无 C# 的 ModuleInitializer，改用 static 初始化块在首次访问时触发
 * {@link RegistryChannels#init()} 完成全部渠道注册。</p>
 *
 * <p>性能：注册仅在类初始化阶段一次性完成，之后是纯读场景（每次 generate/getEmails
 * 均调用 {@link #lookup}）。因此查询用 {@link ConcurrentHashMap} 免锁，
 * {@link #all()} 返回缓存的只读快照避免每次复制。</p>
 */
public final class Registry {

    private static final List<ChannelSpec> ORDERED = new ArrayList<>();
    private static final Map<String, ChannelSpec> MAP = new ConcurrentHashMap<>();
    private static final Object LOCK = new Object();
    // 缓存的只读有序快照，注册完成后即稳定；注册阶段每次追加后刷新
    private static volatile List<ChannelSpec> snapshot = Collections.emptyList();

    static {
        // 首次加载 Registry 类时触发全部渠道注册（幂等）
        RegistryChannels.init();
    }

    private Registry() {
    }

    /**
     * 获取有序渠道注册表（只读快照）。
     *
     * @return 渠道规格列表
     */
    public static List<ChannelSpec> all() {
        // 返回缓存的只读快照，避免每次调用复制整个列表
        return snapshot;
    }

    /**
     * 注册一个渠道。重复注册同一标识视为编程错误，直接抛出。
     *
     * @param spec 渠道规格
     */
    public static void register(ChannelSpec spec) {
        synchronized (LOCK) {
            if (MAP.putIfAbsent(spec.getChannel(), spec) != null) {
                throw new IllegalStateException("duplicate channel registration: " + spec.getChannel());
            }
            ORDERED.add(spec);
            // 注册阶段刷新只读快照，注册结束后 all() 直接复用无需再复制
            snapshot = Collections.unmodifiableList(new ArrayList<>(ORDERED));
        }
    }

    /**
     * 按标识查找渠道规格。
     *
     * @param channel 渠道标识
     * @return 渠道规格，未找到返回 null
     */
    public static ChannelSpec lookup(String channel) {
        // ConcurrentHashMap 免锁读，热路径（每次 generate/getEmails 均调用）
        return MAP.get(channel);
    }
}
