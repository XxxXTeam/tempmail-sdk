package com.xxxxteam.tempmail

/**
 * 渠道注册表
 *
 * 使用反射自动发现 providers 包下的所有 Provider 实现，
 * 也支持手动注册。渠道列表顺序与 Go SDK allChannels 对齐。
 */
object Registry {
    private val channels = mutableListOf<ChannelSpec>()
    // 使用并发 map，读多写少场景下无锁读取
    private val providers = java.util.concurrent.ConcurrentHashMap<String, Provider>()

    // 只读快照缓存：注册阶段结束后被反复读取，缓存避免每次调用重复分配集合
    @Volatile
    private var specsSnapshot: List<ChannelSpec> = emptyList()

    @Volatile
    private var channelIdsSnapshot: List<String> = emptyList()

    /** 注册一个渠道（注册期间加锁保证 channels 列表与快照一致） */
    @Synchronized
    fun register(spec: ChannelSpec, provider: Provider) {
        // 显式使用 containsKey，避免 ConcurrentHashMap 的 !in 误调 containsValue
        if (!providers.containsKey(spec.channel)) {
            channels.add(spec)
            providers[spec.channel] = provider
            rebuildSnapshots()
        }
    }

    /** 重建只读快照（仅在注册/清空时调用） */
    private fun rebuildSnapshots() {
        specsSnapshot = channels.toList()
        channelIdsSnapshot = channels.map { it.channel }
    }

    /** 获取全部渠道列表（有序，只读快照） */
    fun all(): List<ChannelSpec> = specsSnapshot

    /** 获取渠道数量 */
    fun size(): Int = specsSnapshot.size

    /** 根据渠道标识获取 Provider */
    fun getProvider(channel: String): Provider? = providers[channel]

    /** 获取全部渠道标识列表（只读快照） */
    fun listChannels(): List<String> = channelIdsSnapshot

    /** 清空注册表（仅测试用） */
    @Synchronized
    internal fun clear() {
        channels.clear()
        providers.clear()
        rebuildSnapshots()
    }
}
