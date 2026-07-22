package com.xxxxteam.tempmail

/**
 * TempMail SDK 主入口
 *
 * 提供两种使用方式：
 * 1. 无状态函数式 API（generateEmail / getEmails）
 * 2. 有状态 Client 封装（自动管理 token）
 *
 * 示例：
 * ```kotlin
 * // 函数式
 * val info = TempMail.generateEmail("tempmail")
 * val emails = TempMail.getEmails(info)
 *
 * // Client 封装
 * val client = TempMail.client("tempmail")
 * val addr = client.generate()
 * val mails = client.getEmails()
 * ```
 */
object TempMail {

    /** 初始化：触发渠道注册 */
    init {
        RegistryChannels.init()
    }

    /**
     * 生成临时邮箱
     *
     * @param channel 渠道标识，为空时随机选择
     * @return 邮箱信息
     * @throws IllegalArgumentException 渠道不存在
     */
    suspend fun generateEmail(channel: String = ""): EmailInfo {
        val ch = channel.ifEmpty { Registry.listChannels().random() }
        val provider = Registry.getProvider(ch)
            ?: throw IllegalArgumentException("渠道不存在: $ch")
        val info = provider.generate()
        Telemetry.reportUsage(ch, "generate")
        return info
    }

    /**
     * 获取邮件列表
     *
     * @param info 由 generateEmail 返回的邮箱信息
     * @return 邮件列表
     */
    suspend fun getEmails(info: EmailInfo): List<Email> {
        val provider = Registry.getProvider(info.channel)
            ?: throw IllegalArgumentException("渠道不存在: ${info.channel}")
        val emails = provider.getEmails(info)
        Telemetry.reportUsage(info.channel, "getEmails")
        return emails
    }

    /**
     * 带 fallback 的邮箱生成
     *
     * 指定渠道失败后随机尝试其他渠道
     *
     * @param channel 首选渠道
     * @param maxFallback 最大 fallback 次数
     */
    suspend fun generateWithFallback(channel: String = "", maxFallback: Int = 3): EmailInfo {
        val ch = channel.ifEmpty { Registry.listChannels().random() }
        try {
            return generateEmail(ch)
        } catch (_: Exception) {
            // fallback
        }
        val candidates = Registry.listChannels().filter { it != ch }.shuffled()
        for (i in 0 until minOf(maxFallback, candidates.size)) {
            try {
                return generateEmail(candidates[i])
            } catch (_: Exception) {
                continue
            }
        }
        throw RuntimeException("所有渠道均失败")
    }

    /** 获取全部渠道列表 */
    fun listChannels(): List<ChannelSpec> = Registry.all()

    /** 配置 SDK（DSL 风格） */
    fun configure(block: SdkConfig.() -> Unit) = Config.configure(block)

    /**
     * 创建有状态客户端
     *
     * @param channel 渠道标识
     */
    fun client(channel: String = ""): Client = Client(channel)
}

/**
 * 有状态客户端
 *
 * 封装 EmailInfo 和内部 token，用户无需手动传递状态。
 */
class Client(private val channel: String) {
    private var info: EmailInfo? = null

    /** 生成邮箱地址 */
    suspend fun generate(): EmailInfo {
        val result = TempMail.generateEmail(channel)
        info = result
        return result
    }

    /** 获取邮件列表 */
    suspend fun getEmails(): List<Email> {
        val currentInfo = info ?: throw IllegalStateException("请先调用 generate() 生成邮箱")
        return TempMail.getEmails(currentInfo)
    }

    /** 当前邮箱地址 */
    val email: String? get() = info?.email
}
